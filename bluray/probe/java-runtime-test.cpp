// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/apps/mplayerc/BlurayJavaRuntime.h"
#include <cassert>
#include <iostream>

using namespace BlurayJava;

static void Dll(const fs::path& path, WORD machine) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    IMAGE_DOS_HEADER dos{}; dos.e_magic = IMAGE_DOS_SIGNATURE; dos.e_lfanew = sizeof(dos);
    IMAGE_FILE_HEADER header{}; header.Machine = machine; header.Characteristics = IMAGE_FILE_DLL;
    DWORD signature = IMAGE_NT_SIGNATURE;
    file.write(reinterpret_cast<const char*>(&dos), sizeof(dos));
    file.write(reinterpret_cast<const char*>(&signature), sizeof(signature));
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
}
static void Release(const fs::path& home, const std::string& version) {
    std::ofstream(home / L"release", std::ios::binary) << "JAVA_VERSION=\"" << version
        << "\"\r\nIMPLEMENTOR=\"Eclipse Adoptium\"\r\nIMPLEMENTOR_VERSION=\"Temurin-" << version << "\"\r\n";
}
static void Java(const fs::path& home, const std::string& version = "21.0.12.1", WORD machine = IMAGE_FILE_MACHINE_AMD64) {
    Dll(home / L"bin/server/jvm.dll", machine);
    Dll(home / L"bin/awt.dll", machine);
    fs::create_directories(home / L"lib");
    std::ofstream(home / L"lib/modules") << "fixture";
    Release(home, version);
}
int wmain(int argc, wchar_t** argv) {
    assert(argc >= 2);
    const fs::path root(argv[1]);
    assert(root.is_absolute() && !fs::exists(root));
    fs::create_directories(root);
    Java(root / L"portable/jre");
    Java(root / L"portable/java");
    Java(root / L"system");
    Java(root / L"manual");
    Java(root / L"java8", "1.8.0_391");
    Java(root / L"java25", "25.0.1");
    Java(root / L"x86", "21.0.12.1", IMAGE_FILE_MACHINE_I386);
    Java(root / L"arm", "21.0.12.1", IMAGE_FILE_MACHINE_ARM64);
    Java(root / L"headless"); fs::remove(root / L"headless/bin/awt.dll");
    Java(root / L"unknown"); fs::remove(root / L"unknown/release");
    Java(root / L"broken"); std::ofstream(root / L"broken/bin/server/jvm.dll") << "invalid";
    Java(root / L"nested/jre");
    auto selected = Inspect(root / L"portable/jre");
    assert(selected.Valid() && selected.bits == 64 && selected.vendor == L"Eclipse Temurin");
    assert(selected.version == L"21.0.12.1");
    assert(Inspect(root / L"nested").Valid());
    assert(Inspect(L"relative").error == Error::BadPath);
    assert(Inspect(root / L"missing").error == Error::NotFound);
    assert(Inspect(root / L"java8").error == Error::WrongVersion);
    assert(Inspect(root / L"java25").error == Error::WrongVersion);
    assert(Inspect(root / L"x86").error == Error::WrongArchitecture);
    assert(Inspect(root / L"arm").error == Error::WrongArchitecture);
    assert(Inspect(root / L"headless").error == Error::MissingAwt);
    assert(Inspect(root / L"unknown").error == Error::UnknownVersion);
    assert(Inspect(root / L"broken").error == Error::InvalidJvm);
    assert(ReleaseValue("NOT_JAVA_VERSION=\"21\"\nJAVA_VERSION=\"25\"", "JAVA_VERSION") == L"25");
    assert(Major(L"210") == 210 && Major(L"21bad") == 0 && Major(L"21-ea") == 21);
    const std::vector<fs::path> candidates{root / L"portable/jre", root / L"portable/java", root / L"java8", root / L"system"};
    assert(SamePath(Select({}, candidates).home, root / L"portable/jre"));
    assert(SamePath(Select(root / L"manual", candidates).home, root / L"manual"));
    assert(!Select(root / L"java25", candidates).Valid());
    assert(!Select(root / L"missing", candidates).Valid());
    Release(root / L"portable/jre", "25");
    assert(SamePath(Select({}, candidates).home, root / L"portable/java"));
    Release(root / L"portable/java", "8");
    assert(SamePath(Select({}, candidates).home, root / L"system"));
    assert(!Select({}, {root / L"java8", root / L"java25", root / L"x86", root / L"headless"}).Valid());
    assert(!NeedsRestart(selected, {}) && !NeedsRestart(selected, selected));
    assert(NeedsRestart(Inspect(root / L"manual"), selected));
    assert(NeedsRestart({}, selected));
    // A moved portable player resolves its own jre; no absolute selection is persisted.
    Java(root / L"moved/jre");
    const auto moved = Candidates(root / L"moved");
    assert(SamePath(Select({}, moved).home, root / L"moved/jre"));
    // PATH lookup follows java.exe symlinks where available; never searches CWD.
    const auto previous = Environment(L"PATH");
    std::ofstream(root / L"system/bin/java.exe") << "fixture";
    SetEnvironmentVariableW(L"PATH", (L".;\"" + (root / L"system/bin").wstring() + L"\"").c_str());
    const auto found = Candidates(root / L"portable");
    assert(std::any_of(found.begin(), found.end(), [&](const auto& home) { return SamePath(home, root / L"system"); }));
    SetEnvironmentVariableW(L"PATH", previous.c_str());
    assert(Loaded().home.empty()); // Inspection has not loaded any JVM.
    if (argc >= 3) {
        const auto real = Inspect(argv[2]);
        assert(real.Valid() && real.bits == 64 && Major(real.version) == 21);
        std::cout << "Installed Java 21 file inspection: passed\n";
    }
    std::cout << "Java selection, validation, portable relocation, PATH and restart regressions: passed\n";
    return 0;
}
