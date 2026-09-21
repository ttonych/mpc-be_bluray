"""Validate real-time holds and graph reuse in a Blu-ray player navigation log."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('log', type=Path)
parser.add_argument('--minimum-holds', type=int, default=3)
args = parser.parse_args()
holds = []
active = None
playlist = None
attachments = 0
next_item = None
for line in args.log.read_text(encoding='utf-8').splitlines():
    fields = line.split()
    if len(fields) != 4:
        continue
    tick, event, a, b = fields
    tick, a, b = int(tick), int(a), int(b)
    if event == 'playlist':
        playlist = a
        active = None  # A menu command can intentionally leave a still.
        attachments = 0
        next_item = None
    elif event == 'madvr_attached':
        assert a == 1, 'madVR attachment failed'
        attachments += 1
    elif event == 'player_seek_queued':
        active = None  # Explicit seeking cancels the current hold.
        next_item = None
    elif event == 'playback_failed':
        raise AssertionError(f'Could not configure playback: {a}')
    elif event == 'still_hold':
        assert active is None, 'Overlapping still holds'
        assert b > 0, 'Timed hold has no duration'
        assert attachments == 1, 'Graph rebuilt between pictures'
        active = dict(playlist=playlist, playitem=a, seconds=b,
                      start_ms=tick, attachments=attachments)
    elif event == 'still_release':
        assert active is not None, 'Release without a hold'
        assert a == active['playitem'] and b == active['seconds']
        elapsed = tick - active['start_ms']
        assert b * 1000 <= elapsed <= b * 1000 + 1000, (a, elapsed, b)
        assert attachments == active['attachments'], 'Graph rebuilt during hold'
        active['elapsed_ms'] = elapsed
        holds.append(active)
        active = None
        next_item = a + 1
    elif event == 'event':
        assert a not in (1, 2, 3), f'Navigation error {a}, parameter {b}'
        if a == 7 and active:
            assert b == active['playitem'], 'Navigator advanced during hold'
        if a == 7 and next_item is not None:
            assert b == next_item, f'Expected play item {next_item}, got {b}'
            next_item = None

assert len(holds) >= args.minimum_holds, f'Only {len(holds)} completed holds'
print(json.dumps({'completed_holds': len(holds), 'holds': holds}, indent=2))
