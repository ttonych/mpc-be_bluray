// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Read-only sources and an atomic destination commit. No CProfile migration API:
// that API moves settings and removes the original profile.
#include <windows.h>
#include <wincrypt.h>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <cstdint>

namespace PortableProfile {
namespace fs = std::filesystem;
struct Less {
    bool operator()(const std::wstring& a, const std::wstring& b) const {
        return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_LESS_THAN;
    }
};
using Section = std::map<std::wstring, std::wstring, Less>;
using Snapshot = std::map<std::wstring, Section, Less>;
inline bool Equal(const std::wstring& a, const std::wstring& b) { return !Less{}(a,b) && !Less{}(b,a); }
inline bool Starts(const std::wstring& a, const std::wstring& b) { return a.size() >= b.size() && Equal(a.substr(0,b.size()),b); }
struct File {
    HANDLE h = INVALID_HANDLE_VALUE;
    explicit File(HANDLE value) : h(value) {}
    ~File() { if (h != INVALID_HANDLE_VALUE) CloseHandle(h); }
    File(const File&) = delete;
    File& operator=(const File&) = delete;
};
struct Key {
    HKEY h = nullptr;
    ~Key() { if (h) RegCloseKey(h); }
};
inline void Require(bool ok) { if (!ok) throw std::runtime_error("Profile import failed"); }
inline std::vector<BYTE> ReadFileBytes(const fs::path& path) {
    File file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    Require(file.h != INVALID_HANDLE_VALUE);
    LARGE_INTEGER size{};
    Require(GetFileSizeEx(file.h, &size) && size.QuadPart >= 0 && size.QuadPart <= 32 * 1024 * 1024);
    std::vector<BYTE> data(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    Require(data.empty() || (ReadFile(file.h, data.data(), static_cast<DWORD>(data.size()), &read, nullptr) && read == data.size()));
    return data;
}
inline std::wstring Decode(const std::vector<BYTE>& data) {
    if (data.size() >= 2 && data[0] == 0xff && data[1] == 0xfe) {
        Require(data.size() % 2 == 0);
        std::wstring result((data.size()-2)/2, L'\0');
        if (!result.empty()) memcpy(result.data(), data.data()+2, data.size()-2);
        return result;
    }
    const bool utf8 = data.size() >= 3 && data[0] == 0xef && data[1] == 0xbb && data[2] == 0xbf;
    const size_t offset = utf8 ? 3 : 0;
    if (data.size() == offset) return {};
    const char* bytes = reinterpret_cast<const char*>(data.data()+offset);
    const int count = static_cast<int>(data.size()-offset);
    const UINT page = utf8 ? CP_UTF8 : CP_ACP; // same BOM/legacy ANSI policy as CProfile
    const DWORD flags = utf8 ? MB_ERR_INVALID_CHARS : 0;
    int length = MultiByteToWideChar(page, flags, bytes, count, nullptr, 0);
    Require(length > 0);
    std::wstring result(length, L'\0');
    Require(MultiByteToWideChar(page, flags, bytes, count, result.data(), length) == length);
    return result;
}
inline Snapshot ReadIni(const fs::path& path) {
    auto decoded = Decode(ReadFileBytes(path));
    Require(decoded.find(L'\0') == std::wstring::npos);
    Snapshot result;
    std::wistringstream stream(decoded);
    std::wstring line, section;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        if (line.empty() || line[0] == L';') continue;
        if (line[0] == L'[') {
            auto end = line.find(L']');
            if (end != std::wstring::npos) section = line.substr(1,end-1);
        } else {
            auto split = line.find(L'=');
            if (!section.empty() && split != std::wstring::npos && split > 0)
                result[section][line.substr(0,split)] = line.substr(split+1);
        }
    }
    Require(result.count(L"Settings") != 0); // do not accept unrelated INI files
    return result;
}
inline bool HexValue(const std::wstring& section, const std::wstring& name) {
    return (Equal(section,L"OSD") && (Equal(name,L"FontColor") || Equal(name,L"GradColor1") || Equal(name,L"GradColor2")))
        || (Equal(section,L"Theme") && (Equal(name,L"ThemeColor") || Equal(name,L"ToolbarColorFace") || Equal(name,L"ToolbarColorOutline")));
}
inline std::wstring RegistryValue(const std::wstring& section, const std::wstring& name, DWORD type, const std::vector<BYTE>& data) {
    if (type == REG_DWORD && data.size() == 4) {
        DWORD number; memcpy(&number,data.data(),4);
        if (HexValue(section,name)) { wchar_t text[16]; swprintf_s(text,L"%08x",number); return text; }
        // CProfile's wcstoul also accepts the signed spelling modulo 2^32.
        // This preserves both negative ReadInt values and unsigned bit fields.
        return std::to_wstring(static_cast<int32_t>(number));
    }
    if (type == REG_QWORD && data.size() == 8) {
        int64_t number; memcpy(&number,data.data(),8); return std::to_wstring(number);
    }
    if ((type == REG_SZ || type == REG_EXPAND_SZ) && data.size() % 2 == 0) {
        std::wstring text(data.size()/2,L'\0');
        if (!data.empty()) memcpy(text.data(),data.data(),data.size());
        while (!text.empty() && text.back() == L'\0') text.pop_back();
        Require(text.find_first_of(L"\r\n") == std::wstring::npos && text.find(L'\0') == std::wstring::npos);
        return text;
    }
    if (type == REG_BINARY) {
        if (Equal(section,L"Settings\\FullscreenRes")) {
            std::wstring text;
            for (BYTE b : data) { text += wchar_t(L'A'+(b&15)); text += wchar_t(L'A'+(b>>4)); }
            return text;
        }
        if (data.empty()) return {};
        DWORD length = 0;
        Require(CryptBinaryToStringW(data.data(),static_cast<DWORD>(data.size()),CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,nullptr,&length));
        std::wstring text(length,L'\0');
        Require(CryptBinaryToStringW(data.data(),static_cast<DWORD>(data.size()),CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,text.data(),&length));
        text.resize(length);
        return text;
    }
    throw std::runtime_error("Unsupported registry value");
}
inline void ReadRegistryTree(HKEY root, const std::wstring& section, Snapshot& snapshot, unsigned depth = 0) {
    Require(depth <= 16);
    DWORD maxName = 0, maxData = 0;
    Require(RegQueryInfoKeyW(root,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,&maxName,&maxData,nullptr,nullptr) == ERROR_SUCCESS);
    Require(maxData <= 32*1024*1024 && maxName <= 32767);
    for (DWORD index = 0;; ++index) {
        std::vector<wchar_t> name(maxName+2);
        std::vector<BYTE> data(maxData);
        DWORD chars = static_cast<DWORD>(name.size()), size = maxData, type = 0;
        auto status = RegEnumValueW(root,index,name.data(),&chars,nullptr,&type,data.data(),&size);
        if (status == ERROR_NO_MORE_ITEMS) break;
        Require(status == ERROR_SUCCESS);
        data.resize(size);
        if (!section.empty() && chars) {
            std::wstring key(name.data(),chars);
            Require(key.find_first_of(L"=\r\n") == std::wstring::npos);
            snapshot[section][key] = RegistryValue(section,key,type,data);
        }
    }
    for (DWORD index = 0;; ++index) {
        wchar_t name[256]; DWORD chars = 256;
        auto status = RegEnumKeyExW(root,index,name,&chars,nullptr,nullptr,nullptr,nullptr);
        if (status == ERROR_NO_MORE_ITEMS) break;
        Require(status == ERROR_SUCCESS);
        Require(std::wstring(name).find_first_of(L"]\r\n") == std::wstring::npos);
        Key child;
        Require(RegOpenKeyExW(root,name,0,KEY_READ,&child.h) == ERROR_SUCCESS);
        ReadRegistryTree(child.h,section.empty() ? name : section+L"\\"+name,snapshot,depth+1);
    }
}
inline Snapshot ReadRegistry(HKEY root, const wchar_t* path) {
    Key key;
    Require(RegOpenKeyExW(root,path,0,KEY_READ,&key.h) == ERROR_SUCCESS);
    Snapshot result;
    ReadRegistryTree(key.h,L"",result);
    Require(result.count(L"Settings") != 0);
    return result;
}
inline void Prepare(Snapshot& snapshot, const fs::path& sourceDirectory, bool imported) {
    // New BD-J data must never point back to the imported player's storage.
    snapshot[L"Settings"][L"BluRayPersistentRoot"] = L"";
    snapshot[L"Settings"][L"BluRayCacheRoot"] = L"";
    snapshot[L"Settings"][L"BluRayMenus"] = L"1";
    snapshot[L"Settings"][L"MultipleInstances"] = L"2";
    snapshot[L"Settings"][L"KeepHistory"] = L"0";
    snapshot[L"Settings"][L"RememberFilePos"] = L"0";
    snapshot[L"Playlist"][L"RememberMainPlaylist"] = L"0";
    snapshot[L"Settings"][L"UseGlobalMedia"] = L"0";
    snapshot[L"Settings"][L"UseWinLirc"] = L"0";
    snapshot[L"WebServer"][L"EnableWebServer"] = L"0";
    if (!imported) {
        snapshot[L"Video"][L"VideoRenderer"] = L"7";
        snapshot[L"Audio"][L"Volume"] = L"25";
        snapshot[L"OSD"][L"ShowOSD"] = L"5";
        snapshot[L"Settings"][L"ChapterMarker"] = L"1";
    }
    for (auto it = snapshot.begin(); it != snapshot.end();) {
        if (Starts(it->first,L"Recent") || Starts(it->first,L"Favorites") || Starts(it->first,L"File Position") || Starts(it->first,L"DVD Position"))
            it = snapshot.erase(it);
        else {
            if (Starts(it->first,L"ExternalFilters\\") && !sourceDirectory.empty()) {
                auto path = it->second.find(L"Path");
                if (path != it->second.end() && !path->second.empty() && fs::path(path->second).is_relative())
                    path->second = (sourceDirectory/path->second).lexically_normal().wstring();
            }
            ++it;
        }
    }
    snapshot[L"PortableTest"].clear();
    snapshot[L"PortableTest"][L"FirstRunComplete"] = L"1";
}
inline bool NeedsSetup(const fs::path& path) {
    if (!fs::exists(path)) return true;
    if (fs::file_size(path) == 0) return true; // explicit /reset creates an empty file
    auto data = ReadIni(path);
    auto section = data.find(L"PortableTest");
    if (section == data.end()) return false; // preserve existing experimental profiles
    auto pending = section->second.find(L"FirstRunComplete");
    return pending != section->second.end() && pending->second == L"0";
}
inline void Commit(const fs::path& destination, const Snapshot& snapshot) {
    std::wstring text = L"\ufeff; MPC-BE portable test profile\r\n";
    for (const auto& section : snapshot) {
        text += L"["+section.first+L"]\r\n";
        for (const auto& value : section.second) text += value.first+L"="+value.second+L"\r\n";
    }
    // A private same-directory temporary file; no in-place truncation on failure.
    wchar_t temporary[MAX_PATH];
    Require(GetTempFileNameW(destination.parent_path().c_str(),L"mpc",0,temporary) != 0);
    bool ok = false;
    {
        File file(CreateFileW(temporary,GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
        DWORD written = 0, length = static_cast<DWORD>(text.size()*sizeof(wchar_t));
        ok = file.h != INVALID_HANDLE_VALUE && WriteFile(file.h,text.data(),length,&written,nullptr) && written == length && FlushFileBuffers(file.h);
    }
    if (ok) ok = !!MoveFileExW(temporary,destination.c_str(),MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!ok) DeleteFileW(temporary);
    Require(ok);
}
inline void CheckWritable(const fs::path& destination) {
    File file(CreateFileW(destination.c_str(),GENERIC_READ | GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
    Require(file.h != INVALID_HANDLE_VALUE);
}
inline bool SameFile(const fs::path& a, const fs::path& b) {
    std::error_code ec;
    return fs::equivalent(a,b,ec) || Equal(fs::absolute(a).lexically_normal().wstring(),fs::absolute(b).lexically_normal().wstring());
}
// Shader sources are private editable assets. Never copy history, Java caches,
// executables, links, or external filter binaries. Never overwrite destination assets.
inline void CopyShaders(const fs::path& source, const fs::path& destination) {
    if (source.empty() || SameFile(source,destination)) return;
    for (auto name : {L"Shaders",L"Shaders11"}) {
        auto folder = source/name;
        if (!fs::exists(folder)) continue;
        Require(!(GetFileAttributesW(folder.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT));
        for (const auto& entry : fs::directory_iterator(folder)) {
            if (!entry.is_regular_file() || !Equal(entry.path().extension().wstring(),L".hlsl")) continue;
            Require(!(GetFileAttributesW(entry.path().c_str()) & FILE_ATTRIBUTE_REPARSE_POINT));
            fs::create_directories(destination/name);
            fs::copy_file(entry.path(),destination/name/entry.path().filename(),fs::copy_options::skip_existing);
        }
    }
}
}
