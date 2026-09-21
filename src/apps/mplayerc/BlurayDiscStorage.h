// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <objbase.h>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <string>
#include <vector>
#include <array>
#include <cwctype>

// Existing installations keep their shared storage until a particular disc is
// reset. A reset selects a fresh generation for that disc on its next open.
// Nothing is deleted: other discs and the active Java session keep their data.
namespace BlurayDiscStorage {
namespace fs = std::filesystem;

inline bool IsHex(const std::wstring& text, size_t length) {
    return text.size() == length && std::all_of(text.begin(), text.end(), [](wchar_t c) {
        return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f');
    });
}
inline bool ValidKey(const std::wstring& key) {
    return (key.size() == 41 && key[8] == L'-' && IsHex(key.substr(0, 8), 8) && IsHex(key.substr(9), 32))
        || (key.size() == 69 && key.substr(0, 5) == L"hash-" && IsHex(key.substr(5), 64));
}
inline std::wstring Key(const fs::path& disc, const std::string& org, const std::string& id) {
    std::wstring o(org.begin(), org.end()), d(id.begin(), id.end());
    for (auto* s : {&o, &d}) std::transform(s->begin(), s->end(), s->begin(), [](wchar_t c) { return c >= L'A' && c <= L'F' ? c + 32 : c; });
    if (IsHex(o, 8) && IsHex(d, 32) && d.find_first_not_of(L'0') != d.npos) return o + L"-" + d;

    // Decrypted discs sometimes have no certificate ID. Hash navigation
    // metadata, including every playlist, instead of trusting a volume label.
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::wstring result;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return result;
    if (BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0) {
        try {
            std::vector<fs::path> files{L"BDMV/index.bdmv", L"BDMV/MovieObject.bdmv"};
            std::vector<fs::path> playlists;
            for (const auto& e : fs::directory_iterator(disc / L"BDMV/PLAYLIST")) {
                auto ext = e.path().extension().wstring();
                std::transform(ext.begin(), ext.end(), ext.begin(), towlower);
                if (e.is_regular_file() && ext == L".mpls") playlists.push_back(fs::path(L"BDMV/PLAYLIST") / e.path().filename());
            }
            std::sort(playlists.begin(), playlists.end());
            files.insert(files.end(), playlists.begin(), playlists.end());
            bool ok = !playlists.empty();
            uint64_t total = 0;
            std::array<unsigned char, 16384> buffer;
            for (const auto& relative : files) {
                const auto size = fs::file_size(disc / relative);
                total += size;
                if (total > 64 * 1024 * 1024 || size > 4 * 1024 * 1024) { ok = false; break; }
                const std::string name = relative.generic_string();
                if (BCryptHashData(hash, (PUCHAR)name.c_str(), ULONG(name.size() + 1), 0) < 0
                    || BCryptHashData(hash, (PUCHAR)&size, sizeof(size), 0) < 0) { ok = false; break; }
                std::ifstream input(disc / relative, std::ios::binary);
                if (!input) { ok = false; break; }
                uint64_t read = 0;
                while (input.read((char*)buffer.data(), buffer.size()) || input.gcount()) {
                    const auto count = input.gcount(); read += count;
                    if (BCryptHashData(hash, buffer.data(), ULONG(count), 0) < 0) { ok = false; break; }
                }
                if (!ok || read != size) { ok = false; break; }
            }
            unsigned char digest[32];
            if (ok && BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0) {
                static const wchar_t hex[] = L"0123456789abcdef";
                result = L"hash-";
                for (auto b : digest) { result += hex[b >> 4]; result += hex[b & 15]; }
            }
        } catch (const fs::filesystem_error&) { }
        BCryptDestroyHash(hash);
    }
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return result;
}

inline bool Directory(const fs::path& path) {
    if (!CreateDirectoryW(path.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
    const auto attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) && !(attrs & FILE_ATTRIBUTE_REPARSE_POINT);
}
inline bool Resolve(const fs::path& data, const std::wstring& key, fs::path& persistent, fs::path& cache) {
    persistent = data / L"persistent"; cache = data / L"cache";
    if (key.empty()) return true;
    if (!ValidKey(key)) return false;
    const auto marker = data / L"discs" / key / L"active.txt";
    const DWORD attrs = GetFileAttributesW(marker.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) return GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND;
    if (attrs & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) return false;
    std::ifstream input(marker, std::ios::binary);
    char bytes[33]{};
    input.read(bytes, sizeof(bytes));
    if (input.gcount() != 32) return false;
    std::wstring generation(bytes, bytes + 32);
    if (!IsHex(generation, 32)) return false;
    const auto root = marker.parent_path() / generation;
    for (const auto& p : {data, data / L"discs", marker.parent_path(), root, root / L"persistent", root / L"cache"}) {
        const auto a = GetFileAttributesW(p.c_str());
        if (a == INVALID_FILE_ATTRIBUTES || !(a & FILE_ATTRIBUTE_DIRECTORY) || (a & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
    }
    persistent = root / L"persistent"; cache = root / L"cache";
    return true;
}
inline bool Reset(const fs::path& data, const std::wstring& key) {
    if (!ValidKey(key)) return false;
    GUID guid;
    if (FAILED(CoCreateGuid(&guid))) return false;
    wchar_t text[40];
    if (!StringFromGUID2(guid, text, 40)) return false;
    std::wstring generation;
    for (auto c : std::wstring(text)) if (iswxdigit(c)) generation += towlower(c);
    if (!IsHex(generation, 32)) return false;
    const auto folder = data / L"discs" / key;
    const auto root = folder / generation;
    for (const auto& p : {data, data / L"discs", folder, root, root / L"persistent", root / L"cache"}) if (!Directory(p)) return false;
    // Publish atomically, only after both new storage directories exist.
    const auto temporary = folder / (generation + L".txt");
    const std::string bytes(generation.begin(), generation.end());
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const bool ok = WriteFile(file, bytes.data(), DWORD(bytes.size()), &written, nullptr) && written == bytes.size() && FlushFileBuffers(file);
    CloseHandle(file);
    return ok && MoveFileExW(temporary.c_str(), (folder / L"active.txt").c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}
inline bool Resolve(const fs::path& data, const std::wstring& key, fs::path& persistent, fs::path& cache,
                    const fs::path& customPersistent, const fs::path& customCache) {
    if (!Resolve(data,key,persistent,cache)) return false;
    const bool isolated=persistent!=data/L"persistent";
    const auto generation=isolated?persistent.parent_path().filename():fs::path();
    if (!customPersistent.empty()) persistent=isolated?customPersistent/L"mpc-be-discs"/key/generation:customPersistent;
    if (!customCache.empty()) cache=isolated?customCache/L"mpc-be-discs"/key/generation:customCache;
    return true;
}
}
