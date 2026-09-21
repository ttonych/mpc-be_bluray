"""Read playlist timing from the public libbluray API; no disc navigation."""
import ctypes as c
import json
import os
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
dll_dir = os.add_dll_directory(str(root / 'out/libbluray-1.5.0-x64/bin'))
bd = c.CDLL(str(root / 'out/libbluray-1.5.0-x64/bin/bluray-4.dll'))
class Stream(c.Structure):
    _fields_ = [(n, c.c_uint8) for n in ['coding_type', 'format', 'rate', 'char_code']] + [('lang', c.c_uint8 * 4), ('pid', c.c_uint16)] + [(n, c.c_uint8) for n in ['aspect', 'subpath_id', 'dynamic_range_type', 'color_space', 'cr_flag', 'hdr_plus_flag']]
def video_info(clip):
    streams = c.cast(clip.video_streams, c.POINTER(Stream))
    return [{n: getattr(s, n) for n in ['coding_type', 'dynamic_range_type', 'color_space']} for s in streams[:clip.video_stream_count]]
class Clip(c.Structure):
    _fields_ = [('pkt_count', c.c_uint32), ('still_mode', c.c_uint8), ('still_time', c.c_uint16)] + [(n + '_count', c.c_uint8) for n in ['video_stream', 'audio_stream', 'pg_stream', 'ig_stream', 'sec_audio_stream', 'sec_video_stream', 'dv_stream']] + [(n + 's', c.c_void_p) for n in ['video_stream', 'audio_stream', 'pg_stream', 'ig_stream', 'sec_audio_stream', 'sec_video_stream', 'dv_stream']] + [('start_time', c.c_uint64), ('in_time', c.c_uint64), ('out_time', c.c_uint64), ('clip_id', c.c_char * 6)]
class Title(c.Structure):
    _fields_ = [('idx', c.c_uint32), ('playlist', c.c_uint32), ('duration', c.c_uint64), ('clip_count', c.c_uint32), ('angle_count', c.c_uint8), ('chapter_count', c.c_uint32), ('mark_count', c.c_uint32), ('clips', c.POINTER(Clip)), ('chapters', c.c_void_p), ('marks', c.c_void_p), ('mvc', c.c_uint8), ('sdr', c.c_uint8)]
bd.bd_open.argtypes = [c.c_char_p, c.c_char_p]; bd.bd_open.restype = c.c_void_p
bd.bd_close.argtypes = [c.c_void_p]
bd.bd_get_playlist_info.argtypes = [c.c_void_p, c.c_uint32, c.c_uint]; bd.bd_get_playlist_info.restype = c.POINTER(Title)
bd.bd_free_title_info.argtypes = [c.POINTER(Title)]
handle = bd.bd_open(sys.argv[1].encode(), None)
if not handle: raise RuntimeError('Cannot open disc')
try:
    for number in map(int, sys.argv[2:]):
        p = bd.bd_get_playlist_info(handle, number, 0)
        if not p: continue
        t = p.contents
        print(json.dumps({'playlist': number, 'duration': t.duration / 90000, 'clips': [{'id': x.clip_id.decode(), 'start': x.start_time / 90000, 'in': x.in_time / 90000, 'out': x.out_time / 90000, 'still_mode': x.still_mode, 'still_time': x.still_time, 'video': video_info(x)} for x in t.clips[:t.clip_count]]}))
        bd.bd_free_title_info(p)
finally: bd.bd_close(handle)
