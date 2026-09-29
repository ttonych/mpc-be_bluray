// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

// Inspect files without running Java or loading an unverified DLL. The settings
// page and disc startup share this resolver; libbluray must not search again.
namespace BlurayJava {
namespace fs = std::filesystem;
enum class Error { None, NotFound, BadPath, InvalidJvm, WrongArchitecture, UnknownVersion, WrongVersion, MissingAwt };
struct Runtime {
    fs::path home, jvm;
    std::wstring version, vendor;
    Error error = Error::NotFound;
    unsigned bits = 0;
    bool Valid() const { return error == Error::None; }
};
inline fs::path ModulePath(HMODULE module = nullptr) {
    std::wstring buffer(32768, L'\0');
    const DWORD size = GetModuleFileNameW(module, buffer.data(), DWORD(buffer.size()));
    return size && size < buffer.size() ? fs::path(buffer.substr(0, size)) : fs::path();
}
inline bool SamePath(const fs::path& a, const fs::path& b) {
    if (a.empty() || b.empty()) return a.empty() && b.empty();
    std::error_code ec;
    if (fs::equivalent(a, b, ec)) return true;
    return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
}
inline std::wstring Environment(const wchar_t* name) {
    const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
    if (!size || size > 32768) return {};
    std::wstring value(size, L'\0');
    const DWORD count = GetEnvironmentVariableW(name, value.data(), size);
    return count && count < size ? value.substr(0, count) : std::wstring();
}
inline std::wstring Unquote(std::wstring value) {
    const auto start = value.find_first_not_of(L" \t\r\n");
    if (start == value.npos) return {};
    value = value.substr(start, value.find_last_not_of(L" \t\r\n") - start + 1);
    if (value.size() >= 2 && value.front() == L'"' && value.back() == L'"') value = value.substr(1, value.size() - 2);
    return value;
}
inline bool File(const fs::path& path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}
inline WORD Machine(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    IMAGE_DOS_HEADER dos{};
    if (!file.read(reinterpret_cast<char*>(&dos), sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE
        || dos.e_lfanew < LONG(sizeof(dos)) || dos.e_lfanew > 1024 * 1024) return 0;
    file.seekg(dos.e_lfanew);
    DWORD signature = 0;
    IMAGE_FILE_HEADER header{};
    if (!file.read(reinterpret_cast<char*>(&signature), sizeof(signature)) || signature != IMAGE_NT_SIGNATURE
        || !file.read(reinterpret_cast<char*>(&header), sizeof(header)) || !(header.Characteristics & IMAGE_FILE_DLL)) return 0;
    return header.Machine;
}
inline std::wstring ReleaseValue(const std::string& release, const char* key) {
    const std::string prefix = std::string(key) + "=";
    for (size_t begin = 0; begin < release.size();) {
        size_t end = release.find('\n', begin);
        if (end == release.npos) end = release.size();
        if (release.compare(begin, prefix.size(), prefix) == 0) {
            const auto text = release.substr(begin + prefix.size(), end - begin - prefix.size());
            if (text.size() > 256) return {};
            const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), nullptr, 0);
            if (size <= 0) return {};
            std::wstring value(size, L'\0');
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), value.data(), size);
            value = Unquote(value);
            if (value.find_first_of(L"\r\n\t") != value.npos || value.find(L'\0') != value.npos) return {};
            return value;
        }
        begin = end + 1;
    }
    return {};
}
inline unsigned Major(const std::wstring& version) {
    unsigned result = 0;
    size_t i = 0;
    for (; i < version.size() && version[i] >= L'0' && version[i] <= L'9'; ++i) {
        result = result * 10 + version[i] - L'0';
        if (result > 1000) return 0;
    }
    return i && (i == version.size() || version[i] == L'.' || version[i] == L'+' || version[i] == L'-') ? result : 0;
}
inline Runtime Inspect(const fs::path& path) {
    Runtime result;
    result.home = path;
    if (path.empty()) return result;
    if (!path.is_absolute() || path.native().find_first_of(L"\"\r\n") != std::wstring::npos) {
        result.error = Error::BadPath; return result;
    }
    // Match libbluray's precedence for a legacy JDK containing a nested JRE.
    fs::path runtime = File(path / L"jre/bin/server/jvm.dll") ? path / L"jre" : path;
    result.jvm = runtime / L"bin/server/jvm.dll";
    if (!File(result.jvm)) return result;
    const WORD machine = Machine(result.jvm);
    result.bits = machine == IMAGE_FILE_MACHINE_I386 ? 32 : (machine == IMAGE_FILE_MACHINE_AMD64 || machine == IMAGE_FILE_MACHINE_ARM64 ? 64 : 0);
    std::error_code ec;
    fs::path release = path / L"release";
    if (!File(release)) release = runtime / L"release";
    const auto size = fs::file_size(release, ec);
    if (!ec && size <= 65536) {
        std::ifstream file(release, std::ios::binary);
        std::string data(size_t(size), '\0');
        if (file.read(data.data(), std::streamsize(size))) {
            result.version = ReleaseValue(data, "JAVA_VERSION");
            result.vendor = ReleaseValue(data, "IMPLEMENTOR");
            if (ReleaseValue(data, "IMPLEMENTOR_VERSION").find(L"Temurin-") == 0) result.vendor = L"Eclipse Temurin";
        }
    }
    if (!machine) result.error = Error::InvalidJvm;
    else if (machine != IMAGE_FILE_MACHINE_AMD64) result.error = Error::WrongArchitecture;
    else if (!Major(result.version)) result.error = Error::UnknownVersion;
    else if (Major(result.version) != 21) result.error = Error::WrongVersion;
    else if (Machine(runtime / L"bin/awt.dll") != IMAGE_FILE_MACHINE_AMD64
        || !File(runtime / L"lib/modules")) result.error = Error::MissingAwt;
    else result.error = Error::None;
    return result;
}
inline Runtime Select(const fs::path& manual, const std::vector<fs::path>& candidates) {
    if (!manual.empty()) return Inspect(manual); // An explicit choice never silently falls back.
    for (const auto& candidate : candidates) {
        auto runtime = Inspect(candidate);
        if (runtime.Valid()) return runtime;
    }
    return {};
}
inline void Add(std::vector<fs::path>& paths, const fs::path& path) {
    if (!path.is_absolute()) return;
    for (const auto& old : paths) if (SamePath(old, path)) return;
    paths.push_back(path.lexically_normal());
}
inline void RegistryHomes(HKEY root, const std::wstring& key, int depth, std::vector<fs::path>& paths) {
    HKEY handle = nullptr;
    if (RegOpenKeyExW(root, key.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &handle) != ERROR_SUCCESS) return;
    for (const auto* value : {L"JavaHome", L"Path"}) {
        wchar_t text[32768]{};
        DWORD size = sizeof(text), type = 0;
        if (RegQueryValueExW(handle, value, nullptr, &type, reinterpret_cast<BYTE*>(text), &size) == ERROR_SUCCESS
            && type == REG_SZ && size >= sizeof(wchar_t) && size <= sizeof(text) && text[size / sizeof(wchar_t) - 1] == 0)
            Add(paths, Unquote(text));
    }
    if (depth > 0) for (DWORD i = 0; i < 128; ++i) {
        wchar_t child[256]{};
        DWORD size = _countof(child);
        const auto status = RegEnumKeyExW(handle, i, child, &size, nullptr, nullptr, nullptr, nullptr);
        if (status == ERROR_NO_MORE_ITEMS) break;
        if (status == ERROR_SUCCESS) RegistryHomes(handle, child, depth - 1, paths);
    }
    RegCloseKey(handle);
}
inline std::vector<fs::path> Candidates(const fs::path& playerDirectory) {
    std::vector<fs::path> paths;
    Add(paths, playerDirectory / L"jre");
    Add(paths, playerDirectory / L"java");
    Add(paths, Unquote(Environment(L"JAVA_HOME")));
    for (HKEY root : {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE}) {
        for (const auto* key : {L"SOFTWARE\\JavaSoft\\JRE", L"SOFTWARE\\JavaSoft\\JDK",
            L"SOFTWARE\\JavaSoft\\Java Runtime Environment", L"SOFTWARE\\JavaSoft\\Java Development Kit",
            L"SOFTWARE\\Eclipse Adoptium\\JRE", L"SOFTWARE\\Eclipse Adoptium\\JDK",
            L"SOFTWARE\\Eclipse Foundation\\JDK"}) RegistryHomes(root, key, 3, paths);
    }
    const auto env = Environment(L"PATH");
    for (size_t begin = 0; begin < env.size();) {
        size_t end = env.find(L';', begin);
        if (end == env.npos) end = env.size();
        const fs::path bin(Unquote(env.substr(begin, end - begin)));
        if (bin.is_absolute() && File(bin / L"java.exe")) {
            std::error_code ec;
            const auto exe = fs::canonical(bin / L"java.exe", ec);
            if (!ec) Add(paths, exe.parent_path().parent_path());
            Add(paths, bin.parent_path());
        }
        begin = end + 1;
    }
    return paths;
}
inline Runtime Resolve(const fs::path& manual) {
    return manual.empty() ? Select({}, Candidates(ModulePath().parent_path())) : Inspect(manual);
}
inline Runtime Loaded() {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(0, L"jvm.dll", &module)) return {};
    // A mapped DLL alone is not proof that a VM exists (libbluray probes it).
    using GetVMs = LONG (WINAPI*)(void**, LONG, LONG*);
    const auto getVMs = reinterpret_cast<GetVMs>(reinterpret_cast<void*>(GetProcAddress(module, "JNI_GetCreatedJavaVMs")));
    LONG count = 0;
    void* vm = nullptr;
    fs::path home;
    if (getVMs && getVMs(&vm, 1, &count) == 0 && count > 0) home = ModulePath(module).parent_path().parent_path().parent_path();
    FreeLibrary(module);
    return home.empty() ? Runtime{} : Inspect(home);
}
inline bool NeedsRestart(const Runtime& selected, const Runtime& loaded) {
    return !loaded.home.empty() && !SamePath(selected.jvm, loaded.jvm);
}
} // namespace BlurayJava
