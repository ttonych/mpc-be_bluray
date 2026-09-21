"""BABY BOOM component regression: page transitions, controls and blank clicks."""
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
events = [json.loads(line) for line in path.read_text(encoding='utf-8-sig').splitlines()]
checks = {}
summary = next(e for e in reversed(events) if e['type'] == 'summary')
checks['no_navigation_errors'] = summary['errors'] == 0
phases = {}
for event in events:
    phases.setdefault(event.get('phase'), []).append(event)
for phase in ('main-blank', 'scene-blank', 'setup-blank'):
    items = phases.get(phase, [])
    checks[phase] = (any(e['type'] == 'page_return' and e['result'] == 0 for e in items)
                    and not any(e['type'] in ('overlay_flush', 'event') for e in items))
for phase in ('scenes-to-setup', 'setup-to-scenes', 'scenes-to-special',
              'special-to-scenes', 'scenes-to-film'):
    items = phases.get(phase, [])
    checks[phase] = (any(e['type'] == 'page_return' and e['result'] == 1 for e in items)
                    and any(e['type'] == 'mouse_after_return' and e['hit'] == 1 for e in items))
for phase in ('scenes', 'scene-next', 'scene-previous'):
    checks[phase] = any(e['type'] == 'mouse' and e['hit'] == 1 for e in phases.get(phase, []))
# Reference images captured from disc-authored navigation, rather than just
# checking that the new API reports success.
expected_images = {
    'scenes': '3b15c1eb0af269f0',
    'scene-next': 'd69750ed5a8cbcc5',
    'scene-previous': 'b085d4a1606c8b63',
    'scenes-to-setup': '71825efbb58b4a28',
    'setup-to-scenes': '3b15c1eb0af269f0',
    'scenes-to-special': '35037447d87ab99f',
    'special-to-scenes': '3b15c1eb0af269f0',
}
for phase, expected in expected_images.items():
    hashes = [e['hash'] for e in phases.get(phase, []) if e['type'] == 'overlay_flush' and e['plane'] == 1]
    checks[phase + '-image'] = bool(hashes) and hashes[-1] == expected
playlists = [(e['phase'], e['param']) for e in events
             if e['type'] == 'event' and e['name'] == 'PLAYLIST']
checks['film_started_only_by_play_movie'] = playlists == [
    ('first-play', 0), ('first-play', 100), ('first-play', 1), ('scenes-to-film', 2)]
result = {'passed': all(checks.values()), 'checks': checks, 'errors': summary['errors'],
          'scope': 'libbluray component test; no live Windows mouse input or madVR rendering'}
path.with_name('summary.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
print(json.dumps(result, indent=2))
sys.exit(0 if result['passed'] else 1)
