"""Validate real DirectShow/libbluray transition timing for the BABY BOOM test disc."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('log', type=Path)
parser.add_argument('--require-film', action='store_true')
parser.add_argument('--require-controls', action='store_true', help='Require all BABY BOOM audio tracks, subtitles on/off and popup/top menu')
parser.add_argument('--require-reuse', action='store_true', help='Require a same-playlist chapter transition without reopening the graph')
args = parser.parse_args()
events = []
for line in args.log.read_text().splitlines():
    ms, name, a, b = line.split()
    events.append((int(ms), name, int(a), int(b)))
playlists = [e for e in events if e[1] == 'playlist']
expected = [0, 100, 1] + ([2] if args.require_film else [])
actual = [e[2] for e in playlists[:len(expected)]]
assert actual == expected, (actual, expected)
checks = []
for i in range(2):
    start = playlists[i]
    end = next(e for e in events if e[1] == 'graph_complete' and e[2] == start[2] and e[0] > start[0])
    attached = next(e for e in events if e[1] == 'madvr_attached' and e[0] >= start[0])
    assert attached[2] == 1
    assert playlists[i + 1][0] >= end[0], 'Next playlist was selected before DirectShow completed'
    assert abs(end[3] / 1e7 - start[3] / 90000) < .15, 'Playback position did not reach clip end'
    assert (end[0] - attached[0]) / 1000 >= start[3] / 90000 - .15, 'Intro played too quickly'
    checks.append({'playlist': start[2], 'duration_seconds': start[3] / 90000,
                   'position_at_ec_complete_seconds': end[3] / 1e7,
                   'time_since_renderer_attach_seconds': (end[0] - attached[0]) / 1000})
errors = [e for e in events if e[1] == 'event' and e[2] in (1, 2, 3)]
assert not errors, errors
bitmaps = [e for e in events if e[1] == 'madvr_bitmap']
assert bitmaps and all(e[3] == 0 for e in bitmaps)
assert any(e[1:] == ('event', 30, 1) for e in events), 'No interactive menu event'
if args.require_film:
    assert any(e[1:] == ('key', 13, 1) for e in events), 'No successful Enter activation'
report = {'passed': True, 'playlist_sequence': [e[2] for e in playlists], 'intro_checks': checks,
          'madvr_bitmap_calls': len(bitmaps), 'navigation_errors': len(errors)}
selections = [e for e in events if e[1] in ('audio_selected', 'subtitle_selected')]
assert all(e[3] == 0 for e in selections), 'A stream selection failed'
if args.require_controls:
    audio = {e[2] for e in selections if e[1] == 'audio_selected'}
    subtitles = {e[2] for e in selections if e[1] == 'subtitle_selected'}
    assert {0x1100, 0x1101, 0x1102} <= audio, audio
    assert {-1, 0x1200} <= subtitles, subtitles
    assert any(e[1] == 'key' and e[2] == 93 and e[3] >= 0 for e in events), 'Popup command not accepted'
    assert any(e[1:] == ('key', 36, 1) for e in events), 'Top-menu command not accepted'
    report['audio_pids'] = sorted(audio)
    report['subtitle_pids'] = sorted(subtitles)
if args.require_reuse:
    assert any(e[1] == 'playlist_reused' for e in events), 'Graph reuse not exercised'
report['stream_selections'] = len(selections)
report['playlist_reuses'] = sum(e[1] == 'playlist_reused' for e in events)
print(json.dumps(report, indent=2))
