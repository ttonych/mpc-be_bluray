// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "BlurayDiscStorage.h"
#include <map>

// The catalogue stores labels, not Java save formats. Legacy directories may
// contain several discs and must never be presented as an identified disc.
namespace BlurayCatalog {
namespace fs = std::filesystem;
struct Entry {
    std::wstring id, name, alias, source, folder, seen, detail;
    bool legacy = false;
    std::wstring Label() const { return alias.empty() ? name : alias; }
};
inline std::wstring Clean(std::wstring s) {
    for (auto& c : s) if (c < L' ' || c == 0x7f) c = L' ';
    if (s.size() > 32760) s.resize(32760);
    return s;
}
inline bool Id(const std::wstring& id) {
    return BlurayDiscStorage::ValidKey(id) || (id.size() == 71 && id.substr(0,7) == L"legacy-" && BlurayDiscStorage::IsHex(id.substr(7),64));
}
inline std::wstring Hash(const std::wstring& text) {
    unsigned char digest[32];
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return {};
    BCRYPT_HASH_HANDLE hash=nullptr;
    auto status=BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0);
    if (status>=0) {
        status=BCryptHashData(hash,(PUCHAR)text.data(),ULONG(text.size()*sizeof(wchar_t)),0);
        if (status>=0) status=BCryptFinishHash(hash,digest,sizeof(digest),0);
        BCryptDestroyHash(hash);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    if (status < 0) return {};
    std::wstring result;
    for (auto b : digest) { result += L"0123456789abcdef"[b>>4]; result += L"0123456789abcdef"[b&15]; }
    return result;
}
inline bool PlainDirectory(const fs::path& p) {
    const auto a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) && !(a & FILE_ATTRIBUTE_REPARSE_POINT);
}
inline Entry Read(const fs::path& data, const std::wstring& id) {
    Entry e; e.id = id;
    if (!Id(id) || !PlainDirectory(data / L"catalog")) return e;
    const auto file = data / L"catalog" / (id + L".ini");
    const auto a = GetFileAttributesW(file.c_str());
    if (a == INVALID_FILE_ATTRIBUTES || (a & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) return e;
    auto get = [&](const wchar_t* key) {
        std::vector<wchar_t> b(32768);
        GetPrivateProfileStringW(L"Disc", key, L"", b.data(), DWORD(b.size()), file.c_str());
        return Clean(b.data());
    };
    e.name=get(L"Name"); e.alias=get(L"Alias"); e.source=get(L"Source");
    e.folder=get(L"Folder"); e.seen=get(L"LastOpenedUTC"); e.legacy=id.substr(0,7)==L"legacy-";
    return e;
}
inline bool Write(const fs::path& data, const Entry& e) {
    if (!Id(e.id) || !BlurayDiscStorage::Directory(data) || !BlurayDiscStorage::Directory(data/L"catalog")) return false;
    const auto file=data/L"catalog"/(e.id+L".ini");
    const auto attrs=GetFileAttributesW(file.c_str());
    if (attrs!=INVALID_FILE_ATTRIBUTES && (attrs&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))) return false;
    std::wstring content=L"\xfeff[Disc]\r\n";
    auto add=[&](const wchar_t* key,const std::wstring& value) { content+=key; content+=L"="; content+=Clean(value); content+=L"\r\n"; };
    add(L"Name",e.name); add(L"Alias",e.alias); add(L"Source",e.source); add(L"Folder",e.folder); add(L"LastOpenedUTC",e.seen);
    // A unique temporary file and atomic replacement keep concurrent readers safe.
    GUID g; wchar_t token[40]{};
    if (FAILED(CoCreateGuid(&g)) || !StringFromGUID2(g,token,40)) return false;
    const auto temporary=data/L"catalog"/(std::wstring(token)+L".tmp");
    HANDLE f=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (f==INVALID_HANDLE_VALUE) return false;
    DWORD n=0, size=DWORD(content.size()*sizeof(wchar_t));
    bool ok=WriteFile(f,content.data(),size,&n,nullptr)&&n==size&&FlushFileBuffers(f);
    CloseHandle(f);
    return ok&&MoveFileExW(temporary.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
}
inline bool Remember(const fs::path& data,const std::wstring& key,const std::wstring& name,const std::wstring& source,const fs::path& folder) {
    if (!BlurayDiscStorage::ValidKey(key)) return false;
    auto e=Read(data,key); e.name=name.empty()?key:name; e.source=source; e.folder=folder.wstring();
    SYSTEMTIME t; GetSystemTime(&t); wchar_t date[32];
    swprintf_s(date,L"%04u-%02u-%02u %02u:%02u:%02u UTC",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond); e.seen=date;
    return Write(data,e);
}
inline std::wstring FileHint(const fs::path& path) {
    auto stem=path.stem().wstring();
    for (const auto suffix : {L"_pref",L"_book"}) {
        const std::wstring s(suffix);
        if (stem.size()>s.size() && stem.substr(stem.size()-s.size())==s) stem.resize(stem.size()-s.size());
    }
    // Some menus use an ASCII title encoded as hex in their filenames.
    if (stem.size()>=8 && !(stem.size()%2)) {
        auto lower=stem; std::transform(lower.begin(),lower.end(),lower.begin(),towlower);
        if (BlurayDiscStorage::IsHex(lower,lower.size())) {
            std::wstring decoded;
            for (size_t i=0;i<lower.size();i+=2) {
                const unsigned n=unsigned(std::wcstoul(lower.substr(i,2).c_str(),nullptr,16));
                if (n<32 || n>126) { decoded.clear(); break; }
                decoded+=wchar_t(n);
            }
            if (!decoded.empty()) return decoded;
            return {};
        }
    }
    if (stem.find_first_of(L"ghijklmnopqrstuvwxyzGHIJKLMNOPQRSTUVWXYZ _-") == stem.npos) return {};
    if (stem.substr(0,8)==L"bookmark") return {};
    return Clean(stem);
}
inline std::vector<Entry> List(const fs::path& data,const fs::path& legacyRoot) {
    std::map<std::wstring,Entry> entries;
    std::error_code ec;
    if (PlainDirectory(data/L"catalog")) {
        for (fs::directory_iterator it(data/L"catalog",ec),end; !ec&&it!=end;it.increment(ec)) {
            if (it->path().extension()!=L".ini") continue;
            auto id=it->path().stem().wstring(); if (!Id(id)) continue;
            auto e=Read(data,id); if (!e.name.empty()) entries[id]=e;
        }
    }
    // Read file names only. No Java deserialization or guesses from timestamps.
    if (PlainDirectory(legacyRoot)) {
        ec.clear(); unsigned visited=0;
        std::map<std::wstring,std::vector<fs::path>> groups;
        for (fs::recursive_directory_iterator it(legacyRoot,fs::directory_options::skip_permission_denied,ec),end; !ec&&it!=end&&visited++<20000;it.increment(ec)) {
            const auto a=GetFileAttributesW(it->path().c_str());
            if (a==INVALID_FILE_ATTRIBUTES || (a&FILE_ATTRIBUTE_REPARSE_POINT)) { it.disable_recursion_pending(); continue; }
            if (it.depth()>=2) it.disable_recursion_pending();
            if (!(a&FILE_ATTRIBUTE_DIRECTORY) && it.depth()>=1) {
                auto rel=it->path().lexically_relative(legacyRoot); const auto org=rel.begin()->wstring();
                if (BlurayDiscStorage::IsHex(org,8)) groups[it->path().parent_path().wstring()].push_back(it->path());
            }
        }
        for (const auto& [folder,files] : groups) {
            auto norm=folder; std::transform(norm.begin(),norm.end(),norm.begin(),towlower);
            const auto digest=Hash(norm); if (digest.empty()) continue;
            const auto id=L"legacy-"+digest;
            auto e=Read(data,id); e.legacy=true; e.folder=folder;
            const auto relative=fs::path(folder).lexically_relative(legacyRoot).wstring();
            std::wstring hint;
            for (const auto& f:files) {
                auto h=FileHint(f);
                if (!h.empty()) { if (hint.empty()) hint=h; else if (h!=hint) {hint.clear(); break;} }
            }
            e.name=hint.empty()?relative:hint;
            e.detail=relative+L"\r\n";
            for (const auto& f:files) { if (e.detail.size()>4000) break; e.detail+=f.filename().wstring()+L"\r\n"; }
            entries[id]=e;
        }
    }
    std::vector<Entry> result;
    for (auto& [id,e]:entries) result.push_back(std::move(e));
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b) { if (a.legacy!=b.legacy) return !a.legacy; return _wcsicmp(a.Label().c_str(),b.Label().c_str())<0; });
    return result;
}
}
