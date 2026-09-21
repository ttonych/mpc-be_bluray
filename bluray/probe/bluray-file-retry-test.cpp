// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the real file reader with injected Win32 read failures.
#include "../../src/filters/parser/BaseSplitter/stdafx.h"
#include <cassert>
#include <cstdio>

static int failures = 0, reads = 0;
static BOOL WINAPI TestReadFile(HANDLE file, LPVOID data, DWORD size, LPDWORD read, LPOVERLAPPED overlapped)
{
    ++reads;
    if (failures) {
        if (failures > 0) --failures;
        *read = 0;
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    return ::ReadFile(file, data, size, read, overlapped);
}

#define ReadFile TestReadFile
#include "../../src/filters/parser/BaseSplitter/MultiFiles.cpp"
#undef ReadFile

class Reader : public CMultiFiles {
public:
    void SetOffset(REFERENCE_TIME* offset) { m_pCurrentPTSOffset = offset; }
};

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2) return 2;
    HANDLE file = CreateFileW(argv[1], GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    assert(file != INVALID_HANDLE_VALUE);
    const BYTE expected[] = {1, 2, 3, 4};
    DWORD written = 0;
    assert(WriteFile(file, expected, sizeof(expected), &written, nullptr));
    CloseHandle(file);
    Reader reader;
    assert(reader.Open(argv[1]));
    REFERENCE_TIME offset = 99;
    reader.SetOffset(&offset);
    BYTE data[sizeof(expected)]{};
    DWORD error = 0;
    // A raw file has no playlist offsets, but the splitter installs this pointer.
    failures = 1; reads = 0;
    assert(reader.Read(data, sizeof(data), error) == sizeof(data));
    assert(error == ERROR_SUCCESS && offset == 0 && reads == 2);
    assert(!memcmp(data, expected, sizeof(data)));
    reader.Seek(0, FILE_BEGIN);
    failures = -1; reads = 0;
    assert(reader.Read(data, sizeof(data), error) == 0);
    assert(error == ERROR_ACCESS_DENIED && reads == 2);
    failures = 0;
    reader.Close();
    // Playlist reads still use their per-part PTS offsets after a reopen.
    CHdmvClipInfo::CPlaylist items;
    CHdmvClipInfo::PlaylistItem a;
    a.m_strFileName = argv[1]; a.m_rtIn = 100; a.m_rtOut = 200;
    items.push_back(a);
    a.m_rtIn = 400; a.m_rtOut = 500; a.m_rtStartTime = 100;
    items.push_back(a);
    assert(reader.OpenFiles(items));
    reader.SetOffset(&offset);
    assert(reader.Seek(sizeof(data), FILE_BEGIN) == sizeof(data));
    assert(offset == -200);
    failures = 1; reads = 0;
    assert(reader.Read(data, sizeof(data), error) == sizeof(data));
    assert(error == ERROR_SUCCESS && offset == -200 && reads == 2);
    reader.Close();
    assert(DeleteFileW(argv[1]));
    puts("File reader: raw-file retry, persistent failure bound, successful recovery and playlist offsets passed.");
}
