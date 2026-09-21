"""Reconstruct diagnostic previews from a video frame and captured IG layers.

This is an offline image reconstruction, NOT a screenshot of MPC-BE/madVR.
Requires Pillow and an ffmpeg executable on PATH (or --ffmpeg).
"""
import argparse
import json
from pathlib import Path
import subprocess

from PIL import Image

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("evidence", type=Path)
parser.add_argument("--disc", type=Path, default=Path("V:/"))
parser.add_argument("--ffmpeg", default="ffmpeg")
args = parser.parse_args()
events = [json.loads(line) for line in (args.evidence / "events.jsonl").read_text(encoding="utf-8-sig").splitlines()]
summary = next(e for e in reversed(events) if e["type"] == "summary")
playlist = next(e for e in events if e["type"] == "playlist_info" and e["playlist"] == summary["menu_playlist"])
clip = args.disc / "BDMV" / "STREAM" / (playlist["first_clip"] + ".m2ts")
background = args.evidence / "menu-background.png"

def ffmpeg(*options):
    subprocess.run([args.ffmpeg, "-hide_banner", "-loglevel", "error", "-nostdin", *map(str, options)], check=True)

ffmpeg("-i", clip, "-map", "0:v:0", "-frames:v", "1", "-update", "1", "-y", background)
for path in sorted(args.evidence.glob("menu-*.tga")):
    png = path.with_suffix(".png")
    with Image.open(path) as image:
        image.save(png)
    if "first-play" in path.stem or "right" in path.stem:
        output = args.evidence / (path.stem + "-preview.png")
        ffmpeg("-i", background, "-i", png, "-filter_complex", "[0:v][1:v]overlay=0:0:format=auto",
               "-frames:v", "1", "-update", "1", "-y", output)
        print(output)
