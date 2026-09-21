"""Check BD-J playlist sequencing/timing; screenshots must be reviewed separately."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('log', type=Path)
parser.add_argument('--startup', type=int, nargs='+', required=True,
                    help='Expected intro playlists followed by the menu playlist')
parser.add_argument('--loader', action='store_true',
                    help='Require a prepared renderer and visible Java graphics before the first video playlist')
args = parser.parse_args()
events = []
for line in args.log.read_text().splitlines():
    ms, name, a, b = line.split()
    events.append((int(ms), name, int(a), int(b)))
playlists = [e for e in events if e[1] == 'playlist']
assert [e[2] for e in playlists[:len(args.startup)]] == args.startup
assert any(e[1] == 'title_mode' and e[3] == 1 for e in events), 'BD-J not active'
assert any(e[1] == 'argb_frame' for e in events), 'No Java graphics published'
errors = [e for e in events if e[1] == 'event' and e[2] in (1, 2, 3)]
assert not errors, errors
bitmaps = [e for e in events if e[1] == 'madvr_bitmap']
assert bitmaps and all(e[3] == 0 for e in bitmaps), 'Bitmap upload failed'
loader = None
if args.loader:
    canvas = next(i for i, e in enumerate(events) if e[1] == 'bdj_canvas')
    renderer = next(i for i, e in enumerate(events) if e[1] == 'madvr_attached' and e[2] == 1)
    ready = next(i for i, e in enumerate(events) if e[1] == 'bdj_startup_ready')
    first_play = next(i for i, e in enumerate(events) if e[1] == 'first_play')
    first_video = next(i for i, e in enumerate(events) if e[1] == 'playlist')
    visible = next(i for i, e in enumerate(events) if e[1] == 'bdj_graphics_visible' and e[2] == 1)
    upload = next(i for i, e in enumerate(events) if i > visible and e[1] == 'madvr_bitmap' and e[2] == 1)
    assert canvas < renderer < ready < first_play < visible < upload < first_video, 'Startup graphics were not presented before video'
    loader = dict(renderer_ms=events[renderer][0], first_play_ms=events[first_play][0],
                  graphic_visible_ms=events[visible][0], graphic_submitted_ms=events[upload][0],
                  first_video_ms=events[first_video][0])
checks = []
for i, start in enumerate(playlists[:len(args.startup) - 1]):
    next_start = playlists[i + 1][0]
    complete = next(e for e in events if e[1] == 'graph_complete'
                    and e[2] == start[2] and start[0] < e[0] <= next_start)
    renderer = next(e for e in events if e[1] == 'madvr_attached'
                    and start[0] <= e[0] < complete[0])
    assert renderer[2] == 1, 'madVR missing'
    duration = start[3] / 90000
    position = complete[3] / 10000000
    elapsed = (complete[0] - renderer[0]) / 1000
    assert abs(duration - position) < .15, (start[2], duration, position)
    assert elapsed >= duration - .2, (start[2], duration, elapsed)
    checks.append(dict(playlist=start[2], duration_seconds=duration,
                       end_position_seconds=position, elapsed_seconds=elapsed))
print(json.dumps(dict(timing_and_navigation_passed=True,
                     visual_verification='separate live screenshots required',
                     playlist_sequence=[e[2] for e in playlists],
                     loader_check=loader,
                     intro_checks=checks, bitmap_calls=len(bitmaps),
                     native_navigation_errors=len(errors)), indent=2))
