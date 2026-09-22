"""Compile the actual process-query hook against guarded Win32 test doubles."""
import argparse
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, default=root / 'src/apps/mplayerc/mplayerc.cpp')
parser.add_argument('--compiler', type=Path)
args = parser.parse_args()
compiler = args.compiler or Path(os.environ.get('MPCBE_MSYS', root / 'bluray/tools/msys')) / 'mingw/bin/g++.exe'
source = args.source.read_text(encoding='utf-8-sig')
start = source.index('NTSTATUS(WINAPI* Real_NtQueryInformationProcess)')
end = source.index('\n}\n', start) + 3
hook = source[start:end]
# Also accept the previous implementation to verify that this regression fails it.
legacy = ''
if 'typedef struct _PEB_FREE_BLOCK' in source:
    begin = source.index('typedef struct _PEB_FREE_BLOCK')
    legacy = source[begin:source.index('} PEB_NT, *PPEB_NT;', begin) + len('} PEB_NT, *PPEB_NT;')]

fixture = r'''
#include <windows.h>
#include <winternl.h>
#include <array>
#include <cassert>
#include <cstring>
#include <iostream>
static NTSTATUS result;
static unsigned writes;
static std::array<BYTE,4096> peb;
static NTSTATUS WINAPI Query(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG) { return result; }
static BOOL WINAPI Read(HANDLE, LPCVOID address, LPVOID buffer, SIZE_T size, SIZE_T*) {
    assert(address == peb.data() && size <= peb.size());
    memcpy(buffer,address,size);
    // Model a concurrent update after the snapshot. The hook must preserve it.
    peb[0x80] = 0x6b;
    return TRUE;
}
static BOOL WINAPI Write(HANDLE, LPVOID address, LPCVOID buffer, SIZE_T size, SIZE_T*) {
    ++writes;
    assert(address == peb.data()+offsetof(PEB,BeingDebugged) && size == 1);
    memcpy(address,buffer,size);
    return TRUE;
}
#define ReadProcessMemory Read
#define WriteProcessMemory Write
// HOOK
int main() {
    Real_NtQueryInformationProcess=Query;
    peb.fill(0xa5);
    PROCESS_BASIC_INFORMATION info{};
    info.PebBaseAddress=reinterpret_cast<PPEB>(peb.data());
    assert(Mine_NtQueryInformationProcess(nullptr,ProcessBasicInformation,&info,sizeof(info),nullptr)==0);
    assert(writes==1 && peb[offsetof(PEB,BeingDebugged)]==0);
    for (size_t i=0;i<peb.size();++i) if(i!=offsetof(PEB,BeingDebugged)) assert(peb[i]==0xa5);
    writes=0; result=static_cast<NTSTATUS>(0xc0000004);
    assert(Mine_NtQueryInformationProcess(nullptr,ProcessBasicInformation,&info,sizeof(info),nullptr)==result);
    assert(writes==0);
    result=0;
    Mine_NtQueryInformationProcess(nullptr,ProcessBasicInformation,&info,sizeof(info)-1,nullptr);
    Mine_NtQueryInformationProcess(nullptr,ProcessBasicInformation,nullptr,sizeof(info),nullptr);
    info.PebBaseAddress=nullptr;
    Mine_NtQueryInformationProcess(nullptr,ProcessBasicInformation,&info,sizeof(info),nullptr);
    assert(writes==0);
    struct { ULONG_PTR port; ULONG_PTR guard; } data{~ULONG_PTR(0),0xabcdef};
    Mine_NtQueryInformationProcess(nullptr,static_cast<PROCESSINFOCLASS>(7),&data.port,sizeof(data.port),nullptr);
    assert(data.port==0 && data.guard==0xabcdef);
    data.port=~ULONG_PTR(0);
    Mine_NtQueryInformationProcess(nullptr,static_cast<PROCESSINFOCLASS>(7),&data.port,sizeof(data.port)-1,nullptr);
    assert(data.port==~ULONG_PTR(0));
    result=static_cast<NTSTATUS>(0xc0000001);
    Mine_NtQueryInformationProcess(nullptr,static_cast<PROCESSINFOCLASS>(7),&data.port,sizeof(data.port),nullptr);
    assert(data.port==~ULONG_PTR(0));
    result=0;
    Mine_NtQueryInformationProcess(nullptr,static_cast<PROCESSINFOCLASS>(1),&data.port,sizeof(data.port),nullptr);
    assert(data.port==~ULONG_PTR(0) && writes==0);
    std::cout << "PASS: one-byte PEB write, failure/null/length guards, pointer-sized debug port, unrelated queries.\n";
}
'''
build = root / 'bluray/build/tests'
build.mkdir(parents=True, exist_ok=True)
cpp = build / 'process-query-hook-test.cpp'
exe = cpp.with_suffix('.exe')
cpp.write_text(fixture.replace('// HOOK', legacy + '\n' + hook), encoding='utf-8')
subprocess.run([str(compiler), '-std=c++17', '-O2', '-static', str(cpp), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
