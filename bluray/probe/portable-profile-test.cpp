#define NOMINMAX
#include "../../src/apps/mplayerc/PortableProfileImport.h"
#include <iostream>
#include <fstream>

using namespace PortableProfile;
int checks = 0;
void Check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
    ++checks; std::cout << "PASS " << message << '\n';
}
template<class F> void Fails(F operation, const char* message) {
    bool failed = false;
    try { operation(); } catch (const std::exception&) { failed = true; }
    Check(failed,message);
}
void WriteBytes(const fs::path& path, const std::vector<BYTE>& bytes) {
    std::ofstream file(path,std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
}
void RegValue(HKEY root, const wchar_t* section, const wchar_t* name, DWORD type, const void* data, DWORD size) {
    Key key;
    Require(RegCreateKeyExW(root,section,0,nullptr,0,KEY_ALL_ACCESS,nullptr,&key.h,nullptr) == ERROR_SUCCESS);
    Require(RegSetValueExW(key.h,name,0,type,reinterpret_cast<const BYTE*>(data),size) == ERROR_SUCCESS);
}
int wmain(int argc, wchar_t** argv) {
    try {
        Require(argc >= 2);
        fs::path folder(argv[1]); fs::create_directories(folder);
        auto source = folder/L"source.ini", target = folder/L"target.ini";
        Snapshot original;
        original[L"Settings"][L"Language"] = L"ru";
        original[L"Settings"][L"Test Unicode"] = L"Субтитры — 日本語";
        original[L"Settings"][L"BluRayPersistentRoot"] = L"C:\\source\\bdj-data";
        original[L"Settings"][L"BluRayCacheRoot"] = L"C:\\source\\bdj-cache";
        original[L"Settings"][L"UseGlobalMedia"] = L"1";
        original[L"Video"][L"VideoRenderer"] = L"5";
        original[L"Audio"][L"Volume"] = L"43";
        original[L"Commands"][L"CommandMod0"] = L"test keys";
        original[L"ExternalFilters\\0000"][L"Path"] = L"Filters\\decoder.ax";
        Commit(source,original);
        auto baseline = ReadFileBytes(source);
        auto data = ReadIni(source);
        Check(data == original,"UTF-16 and Unicode source round trip");
        Prepare(data,folder,true); Commit(target,data);
        Check(ReadFileBytes(source) == baseline,"INI import leaves source bytes untouched");
        auto imported = ReadIni(target);
        Check(imported[L"Video"][L"VideoRenderer"] == L"5" && imported[L"Audio"][L"Volume"] == L"43" && imported[L"Commands"] == original[L"Commands"],"renderer audio and key bindings retained");
        Check(imported[L"Settings"][L"BluRayPersistentRoot"].empty() && imported[L"Settings"][L"BluRayCacheRoot"].empty(),"BD-J folders reset to private storage");
        Check(imported[L"Settings"][L"UseGlobalMedia"] == L"0" && imported[L"Settings"][L"KeepHistory"] == L"0","shared hotkeys and history off initially");
        Check(fs::path(imported[L"ExternalFilters\\0000"][L"Path"]).is_absolute(),"relative external filter path rebased to source");
        Check(!NeedsSetup(target),"committed profile does not reopen setup");
        Check(!NeedsSetup(source),"existing profile preserved without migration marker");
        Check(NeedsSetup(folder/L"missing.ini"),"missing profile requests setup");
        Snapshot seed; seed[L"Settings"][L"Language"]=L"ru"; seed[L"PortableTest"][L"FirstRunComplete"]=L"0";
        Commit(folder/L"seed.ini",seed);
        Check(NeedsSetup(folder/L"seed.ini"),"distribution seed requests setup");
        WriteBytes(folder/L"empty.ini",{});
        Check(NeedsSetup(folder/L"empty.ini"),"reset profile requests setup");
        std::string utf8 = "\xef\xbb\xbf[Settings]\r\nText=\xd0\xa2\xd0\xb5\xd1\x81\xd1\x82\r\n";
        WriteBytes(folder/L"utf8.ini",std::vector<BYTE>(utf8.begin(),utf8.end()));
        Check(ReadIni(folder/L"utf8.ini")[L"Settings"][L"Text"] == L"Тест","UTF-8 BOM input");
        std::string ansi = "[Settings]\r\nLanguage=en\r\n";
        WriteBytes(folder/L"ansi.ini",std::vector<BYTE>(ansi.begin(),ansi.end()));
        Check(ReadIni(folder/L"ansi.ini")[L"Settings"][L"Language"] == L"en","legacy ANSI input");
        WriteBytes(folder/L"invalid.ini",{0xef,0xbb,0xbf,0xff});
        Fails([&] { ReadIni(folder/L"invalid.ini"); },"malformed Unicode rejected");
        auto before = ReadFileBytes(target);
        Require(SetFileAttributesW(target.c_str(),FILE_ATTRIBUTE_READONLY));
        Fails([&] { Commit(target,seed); },"read-only destination rejects commit");
        Fails([&] { CheckWritable(target); },"read-only profile cannot start");
        Check(ReadFileBytes(target) == before,"failed commit preserves previous destination");
        Require(SetFileAttributesW(target.c_str(),FILE_ATTRIBUTE_NORMAL));
        Check(SameFile(source,source),"same-source destination rejected by identity");
        auto hard = folder/L"hardlink.ini";
        Require(CreateHardLinkW(hard.c_str(),source.c_str(),nullptr));
        Check(SameFile(source,hard),"hard-linked source recognized");
        auto shaderSource = folder/L"shader-source", shaderTarget = folder/L"shader-target";
        fs::create_directories(shaderSource/L"Shaders"); fs::create_directories(shaderTarget/L"Shaders");
        WriteBytes(shaderSource/L"Shaders/test.hlsl",{1,2,3});
        WriteBytes(shaderSource/L"Shaders/ignore.exe",{7});
        WriteBytes(shaderTarget/L"Shaders/test.hlsl",{4,5,6});
        CopyShaders(shaderSource,shaderTarget);
        Check(ReadFileBytes(shaderTarget/L"Shaders/test.hlsl") == std::vector<BYTE>({4,5,6}),"shader copy never overwrites existing assets");
        Check(!fs::exists(shaderTarget/L"Shaders/ignore.exe"),"shader copy excludes executables");

        // Use a unique disposable key. Never modify Software\\MPC-BE in tests.
        std::wstring testKey = L"Software\\MPCBE-PortableImport-Test-"+std::to_wstring(GetCurrentProcessId());
        {
            Key root; Require(RegCreateKeyExW(HKEY_CURRENT_USER,testKey.c_str(),0,nullptr,0,KEY_ALL_ACCESS,nullptr,&root.h,nullptr) == ERROR_SUCCESS);
            DWORD negative = static_cast<DWORD>(-123), color = 0xfecba987, merit = 0x80000001;
            BYTE binary[] = {0,1,0x0f,0xf0,0xfe,0xff};
            int64_t qword = -1234567890123LL;
            const wchar_t text[] = L"Русский";
            RegValue(root.h,L"Settings",L"Delay",REG_DWORD,&negative,4);
            RegValue(root.h,L"Settings",L"Position",REG_QWORD,&qword,8);
            RegValue(root.h,L"Settings",L"Text",REG_SZ,text,sizeof(text));
            RegValue(root.h,L"Settings",L"Binary",REG_BINARY,binary,sizeof(binary));
            RegValue(root.h,L"Settings\\FullscreenRes",L"Res0",REG_BINARY,binary,sizeof(binary));
            RegValue(root.h,L"OSD",L"FontColor",REG_DWORD,&color,4);
            RegValue(root.h,L"ExternalFilters\\0000",L"Merit",REG_DWORD,&merit,4);
            FILETIME timestamp{}; RegQueryInfoKeyW(root.h,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,&timestamp);
            auto fromReg = ReadRegistry(HKEY_CURRENT_USER,testKey.c_str());
            Check(fromReg[L"Settings"][L"Delay"] == L"-123","signed registry DWORD retained");
            Check(wcstoul(fromReg[L"ExternalFilters\\0000"][L"Merit"].c_str(),nullptr,10) == merit,"unsigned high-bit registry DWORD retained");
            Check(fromReg[L"Settings"][L"Position"] == std::to_wstring(qword),"signed QWORD retained");
            Check(fromReg[L"Settings"][L"Text"] == L"Русский","registry Unicode retained");
            Check(fromReg[L"OSD"][L"FontColor"] == L"fecba987","hexadecimal colors converted correctly");
            Check(fromReg[L"Settings\\FullscreenRes"][L"Res0"] == L"AABAPAAPOPPP","legacy fullscreen binary encoding");
            Check(fromReg[L"Settings"][L"Binary"] == L"AAEP8P7/","modern binary base64 encoding");
            Commit(folder/L"registry.ini",fromReg);
            Check(ReadIni(folder/L"registry.ini") == fromReg,"registry snapshot round-trips through INI");
            FILETIME after{}; RegQueryInfoKeyW(root.h,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,&after);
            Check(CompareFileTime(&timestamp,&after) == 0 && ReadRegistry(HKEY_CURRENT_USER,testKey.c_str()) == fromReg,"registry import leaves original data unchanged");
        }
        Require(RegDeleteTreeW(HKEY_CURRENT_USER,testKey.c_str()) == ERROR_SUCCESS);
        Snapshot defaults; Prepare(defaults,{},false);
        Check(defaults[L"Video"][L"VideoRenderer"] == L"7" && defaults[L"PortableTest"][L"FirstRunComplete"] == L"1","clean first-run defaults");
        if (argc > 2) {
            auto actual = ReadRegistry(HKEY_CURRENT_USER,L"Software\\MPC-BE");
            Prepare(actual,{},true); Commit(folder/L"actual-readonly-copy.ini",actual);
            Check(!actual.empty(),"real existing MPC-BE registry read and private snapshot");
        }
        std::cout << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
