#include "../../src/apps/mplayerc/BlurayMediaMonitor.h"
#include <cstdio>
#include <stdexcept>

static int checks = 0;
static void Check(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
    ++checks;
    printf("PASS %s\n", description);
}
static void Write(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE,
        nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("create fixture file");
    DWORD written;
    const char payload[] = "INDX0200-media-monitor-test";
    const bool ok = !!WriteFile(file, payload, sizeof(payload), &written, nullptr);
    CloseHandle(file);
    if (!ok || written != sizeof(payload)) throw std::runtime_error("write fixture");
}
static bool WaitLoss(BlurayMediaMonitor& monitor) {
    const auto until = GetTickCount64() + 3000;
    while (!monitor.LossReason() && GetTickCount64() < until) Sleep(10);
    return monitor.LossReason() != BlurayMediaMonitor::Present;
}
int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 2;
    const std::wstring root = argv[1], dir = root + L"\\BDMV", gone = root + L"\\BDMV-detached";
    const auto index = dir + L"\\index.bdmv", old = dir + L"\\old-index.bdmv";
    try {
        Check(!!CreateDirectoryW(root.c_str(), nullptr), "new isolated fixture directory");
        Check(!!CreateDirectoryW(dir.c_str(), nullptr), "new BDMV directory");
        BlurayMediaMonitor monitor;
        Check(!monitor.Start(root), "missing index cannot start a session");
        Write(index);
        Check(monitor.Start(root), "start on readable disc");
        Sleep(650);
        Check(!monitor.LossReason(), "unchanged media stays active");
        const DWORD own = BlurayMediaMonitor::DriveMask(root);
        Check(own != 0, "local drive identified");
        const DWORD other = own == 1 ? 2 : 1;
        monitor.DeviceChange(DBT_DEVICEREMOVECOMPLETE, other);
        monitor.DeviceChange(DBT_DEVICEARRIVAL, own);
        Sleep(650);
        Check(!monitor.LossReason(), "unrelated removal and duplicate arrival ignored");
        Check(BlurayMediaMonitor::DriveMask(L"v:\\") == (1u << 21), "lowercase drive matching");
        Check(BlurayMediaMonitor::DriveMask(L"\\\\server\\share") == 0, "UNC path has no drive broadcast mask");
        // Same contents, length and timestamp, but a different file identity.
        Check(!!MoveFileW(index.c_str(), old.c_str()), "rename original index while monitored");
        Write(index);
        WIN32_FILE_ATTRIBUTE_DATA attributes{};
        GetFileAttributesExW(old.c_str(), GetFileExInfoStandard, &attributes);
        HANDLE replacement = CreateFileW(index.c_str(), FILE_WRITE_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
        Check(replacement != INVALID_HANDLE_VALUE && !!SetFileTime(replacement, nullptr, nullptr, &attributes.ftLastWriteTime), "preserve replacement timestamp");
        CloseHandle(replacement);
        Check(WaitLoss(monitor) && monitor.LossReason() == BlurayMediaMonitor::Replaced, "replacement detected without a removal interval");
        Check(monitor.Start(root), "new session accepts replacement");
        Check(!!MoveFileW(dir.c_str(), gone.c_str()), "detach source directory");
        Check(WaitLoss(monitor) && monitor.LossReason() == BlurayMediaMonitor::Unavailable, "loss detected without navigation reads or Windows broadcasts");
        Check(!!MoveFileW(gone.c_str(), dir.c_str()), "restore source directory");
        Sleep(550);
        Check(monitor.LossReason() == BlurayMediaMonitor::Unavailable, "reinsertion cannot revive the stale session");
        Check(monitor.Start(root), "explicit reopen starts a clean session");
        monitor.DeviceChange(DBT_DEVICEREMOVECOMPLETE, own);
        Check(monitor.LossReason() == BlurayMediaMonitor::Removed, "same-drive removal immediately latches");
        monitor.Stop();
        Check(!monitor.LossReason(), "stop clears monitor ownership");
        for (int i = 0; i < 20; ++i) {
            if (!monitor.Start(root)) throw std::runtime_error("restart churn");
            monitor.Stop();
        }
        Check(true, "repeated start/stop while worker is pending");
        Sleep(100);
        Check(!!DeleteFileW(index.c_str()) && !!DeleteFileW(old.c_str()), "fixture handles released");
        Check(!!RemoveDirectoryW(dir.c_str()) && !!RemoveDirectoryW(root.c_str()), "fixture cleanup");
        printf("%d checks passed\n", checks);
        return 0;
    } catch (const std::exception& e) {
        fprintf(stderr, "FAIL: %s (Win32=%lu)\n", e.what(), GetLastError());
        return 1;
    }
}
