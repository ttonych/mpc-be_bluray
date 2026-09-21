// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>
#include <dbt.h>
#include <process.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

// Poll metadata off the player/Java threads. Each probe opens a fresh path so
// cached libbluray/file handles cannot disguise replacement media at that path.
class BlurayMediaMonitor {
public:
    enum Reason : DWORD { Present, Removed, Unavailable, Replaced };
    static DWORD DriveMask(const std::wstring& root) {
        if (root.size() < 2 || root[1] != L':') return 0;
        const wchar_t letter = root[0] >= L'a' && root[0] <= L'z' ? root[0] - L'a' + L'A' : root[0];
        return letter >= L'A' && letter <= L'Z' ? DWORD(1) << (letter - L'A') : 0;
    }
private:
    struct Data {
        std::wstring index;
        BY_HANDLE_FILE_INFORMATION initial{};
        HANDLE wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        std::atomic<bool> stopped{false};
        std::atomic<uint64_t> loss{0};
        ~Data() { if (wake) CloseHandle(wake); }
        void Lost(Reason reason, DWORD error = 0) {
            uint64_t expected = 0;
            loss.compare_exchange_strong(expected, (uint64_t(error) << 32) | reason);
        }
    };
    std::shared_ptr<Data> m_data;
    HANDLE m_thread = nullptr;
    DWORD m_driveMask = 0;

    static bool ReadIdentity(const std::wstring& path, BY_HANDLE_FILE_INFORMATION& info, DWORD& error) {
        DWORD previous = 0;
        const BOOL changed = SetThreadErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX, &previous);
        HANDLE file = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
        bool ok = false;
        if (file != INVALID_HANDLE_VALUE) {
            ok = !!GetFileInformationByHandle(file, &info);
            error = ok ? ERROR_SUCCESS : GetLastError();
            CloseHandle(file);
        } else error = GetLastError();
        if (changed) SetThreadErrorMode(previous, nullptr);
        return ok;
    }
    static bool Same(const BY_HANDLE_FILE_INFORMATION& a, const BY_HANDLE_FILE_INFORMATION& b) {
        return a.dwVolumeSerialNumber == b.dwVolumeSerialNumber
            && a.nFileIndexHigh == b.nFileIndexHigh && a.nFileIndexLow == b.nFileIndexLow
            && a.nFileSizeHigh == b.nFileSizeHigh && a.nFileSizeLow == b.nFileSizeLow
            && CompareFileTime(&a.ftLastWriteTime, &b.ftLastWriteTime) == 0;
    }
    static unsigned __stdcall Worker(void* context) {
        std::unique_ptr<std::shared_ptr<Data>> argument(static_cast<std::shared_ptr<Data>*>(context));
        const auto data = *argument;
        while (!data->stopped && !data->loss) {
            WaitForSingleObject(data->wake, 500);
            if (data->stopped) break;
            BY_HANDLE_FILE_INFORMATION current{};
            DWORD error = 0;
            const bool readable = ReadIdentity(data->index, current, error);
            if (data->stopped) break;
            if (!readable) data->Lost(Unavailable, error);
            else if (!Same(data->initial, current)) data->Lost(Replaced);
        }
        return 0;
    }
public:
    BlurayMediaMonitor() = default;
    BlurayMediaMonitor(const BlurayMediaMonitor&) = delete;
    BlurayMediaMonitor& operator=(const BlurayMediaMonitor&) = delete;
    ~BlurayMediaMonitor() { Stop(); }
    bool Start(std::wstring root) {
        Stop();
        m_driveMask = DriveMask(root);
        while (!root.empty() && (root.back() == L'\\' || root.back() == L'/')) root.pop_back();
        auto data = std::make_shared<Data>();
        data->index = root + L"\\BDMV\\index.bdmv";
        DWORD error = 0;
        if (!data->wake || !ReadIdentity(data->index, data->initial, error)) return false;
        auto argument = std::make_unique<std::shared_ptr<Data>>(data);
        m_thread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, Worker, argument.get(), 0, nullptr));
        if (!m_thread) return false;
        argument.release();
        m_data = std::move(data);
        return true;
    }
    void Stop() {
        if (m_data) {
            m_data->stopped = true;
            SetEvent(m_data->wake);
        }
        if (m_thread) {
            CancelSynchronousIo(m_thread);
            CloseHandle(m_thread);
            m_thread = nullptr;
        }
        // Cancellation need not finish immediately. The worker owns its data
        // and never touches the player, so a slow device cannot block teardown.
        m_data.reset();
        m_driveMask = 0;
    }
    void DeviceChange(UINT event, DWORD drives) {
        if (!m_data || !(drives & m_driveMask)) return;
        if (event == DBT_DEVICEREMOVEPENDING || event == DBT_DEVICEREMOVECOMPLETE) m_data->Lost(Removed);
        else if (event == DBT_DEVICEARRIVAL) SetEvent(m_data->wake);
    }
    Reason LossReason() const { return m_data ? Reason(DWORD(m_data->loss.load())) : Present; }
    DWORD Error() const { return m_data ? DWORD(m_data->loss.load() >> 32) : 0; }
};
