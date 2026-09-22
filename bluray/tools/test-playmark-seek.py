"""Exercise libbluray's actual mark tracker with synthetic packet positions."""
import argparse
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, default=root / 'bluray/vendor/libbluray-1.5.0/src/libbluray/bluray.c')
parser.add_argument('--compiler', type=Path)
args = parser.parse_args()
compiler = args.compiler or Path(os.environ.get('MPCBE_MSYS', root / 'bluray/tools/msys')) / 'mingw/bin/g++.exe'
source = args.source.read_text(encoding='utf-8')
functions = []
for name in ('_find_next_playmark', '_playmark_reached'):
    start = source.index('static void ' + name + '(')
    end = source.index('\n}\n', start) + 3
    functions.append(source[start:end])
fixture = r'''
#include <cstdint>
#include <cassert>
#include <iostream>
#include <vector>
struct Mark { uint32_t title_pkt; };
struct Title { struct { unsigned count; Mark* mark; } mark_list; };
struct BLURAY { Title* title; uint64_t s_pos, next_mark_pos; int next_mark; };
static std::vector<int> app_events, java_events;
#define BD_DEBUG(...) ((void)0)
#define BD_EVENT_PLAYMARK 9
#define BDJ_EVENT_MARK 7
static void _queue_event(BLURAY*, int type, int mark) { assert(type==9); app_events.push_back(mark); }
static void _bdj_event(BLURAY*, int type, int mark) { assert(type==7); java_events.push_back(mark); }
static void _update_chapter_psr(BLURAY*) {}
// FUNCTIONS
static void expected(std::initializer_list<int> marks) {
    assert(app_events==std::vector<int>(marks)); assert(java_events==app_events);
    app_events.clear(); java_events.clear();
}
int main() {
    Mark marks[]={{0},{4},{4},{10},{0xffffffffu}};
    Title title{{5,marks}};
    BLURAY bd{&title,4*192,0,-1};
    _find_next_playmark(&bd);
    _playmark_reached(&bd); expected({}); // Position points to unread data.
    ++bd.s_pos; _playmark_reached(&bd); expected({1,2});
    _playmark_reached(&bd); expected({}); // Each occurrence is delivered once.
    _find_next_playmark(&bd); _playmark_reached(&bd); expected({});
    bd.s_pos=0; _find_next_playmark(&bd);
    bd.s_pos=10*192+1; _playmark_reached(&bd); expected({0,1,2,3});
    bd.s_pos=uint64_t(0xffffffffu)*192; _find_next_playmark(&bd);
    _playmark_reached(&bd); expected({});
    ++bd.s_pos; _playmark_reached(&bd); expected({4});
    assert(bd.next_mark==-1);
    _find_next_playmark(&bd); _playmark_reached(&bd); expected({});
    bd.s_pos=5*192; _find_next_playmark(&bd);
    bd.s_pos=11*192; _playmark_reached(&bd); expected({3});
    title.mark_list.count=0; _find_next_playmark(&bd);
    _playmark_reached(&bd); expected({}); assert(bd.next_mark==-1);
    std::cout << "PASS: seek boundary, shared access unit, one-time delivery, backward seek, skipped marks, 64-bit offsets, empty list.\n";
}
'''
build = root / 'bluray/build/tests'
build.mkdir(parents=True, exist_ok=True)
cpp = build / 'playmark-seek-test.cpp'
exe = cpp.with_suffix('.exe')
cpp.write_text(fixture.replace('// FUNCTIONS', '\n'.join(functions)), encoding='utf-8')
subprocess.run([str(compiler), '-std=c++17', '-O2', '-static', str(cpp), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
