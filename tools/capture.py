#!/usr/bin/env python3
"""MAD AI capture pipeline.

Runs a scenario headlessly, encodes every recorded camera into an MP4 (plus a
composite grid), and publishes an HTML report to /Content for human review.

This is the tool an AI agent drives to *see* a gameplay change: author a
scenario, run `tools/capture.py scenarios/foo.mad`, then look at the published
tab. No display required.

Usage:
    tools/capture.py <scenario.mad> [--no-build] [--no-publish]
                     [--frames-dir DIR] [--content-root /Content]
"""
import argparse
import glob
import math
import os
import re
import shutil
import subprocess
import sys
from datetime import datetime, timezone

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TILE_W, TILE_H = 480, 400
TILE_BG = "0x101018"

DEMON_LEGEND = [
    ("Ground", "#dc463c"),
    ("Climber", "#eb9632"),
    ("Flyer", "#78c8fa"),
    ("Smasher", "#b45ad2"),
]


def run(cmd, **kw):
    print("  $", " ".join(cmd))
    return subprocess.run(cmd, check=True, **kw)


def build_release():
    print("==> Building (Release)")
    run([os.path.join(REPO, "build.sh"), "Release"], cwd=REPO,
        stdout=subprocess.DEVNULL)


def record(scenario, frames_dir, trails=False):
    print(f"==> Recording {scenario}")
    os.makedirs(frames_dir, exist_ok=True)
    env = dict(os.environ)
    env["ASAN_OPTIONS"] = "detect_leaks=0"  # harmless if not a sanitized build
    if trails:
        env["MAD_TRAILS"] = "1"
    binary = os.path.join(REPO, "build", "mad")
    run([binary, "--record", scenario, "--out", frames_dir], env=env)


def parse_manifest(frames_dir):
    """Return dict with name, fps, frames, reached_nexus, cameras=[(name,w,h)]."""
    info = {"name": "untitled", "fps": 30.0, "frames": 0,
            "reached_nexus": 0, "cameras": []}
    path = os.path.join(frames_dir, "manifest.txt")
    with open(path) as f:
        for line in f:
            parts = line.rstrip("\n").split("\t")
            if parts[0] == "camera":
                info["cameras"].append((parts[1], int(parts[2]), int(parts[3])))
            elif parts[0] == "name":
                info["name"] = parts[1]
            elif parts[0] == "fps":
                info["fps"] = float(parts[1])
            elif parts[0] in ("frames", "reached_nexus"):
                info[parts[0]] = int(parts[1])
    return info


def encode_camera(frames_dir, cam_name, fps, out_mp4):
    pattern = os.path.join(frames_dir, cam_name, "frame_%06d.ppm")
    run(["ffmpeg", "-y", "-framerate", str(fps), "-i", pattern,
         "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "20",
         "-movflags", "+faststart", out_mp4],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def encode_poster(frames_dir, cam_name, out_png):
    frames = sorted(glob.glob(os.path.join(frames_dir, cam_name, "frame_*.ppm")))
    if not frames:
        return False
    mid = frames[len(frames) // 2]
    run(["ffmpeg", "-y", "-i", mid, out_png],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return True


def encode_composite(frames_dir, cameras, fps, out_mp4):
    """Scale every camera to a uniform tile and xstack them into a grid."""
    n = len(cameras)
    cols = 1 if n == 1 else 2 if n <= 4 else math.ceil(math.sqrt(n))
    inputs = []
    filters = []
    for i, (name, _w, _h) in enumerate(cameras):
        inputs += ["-framerate", str(fps), "-i",
                   os.path.join(frames_dir, name, "frame_%06d.ppm")]
        filters.append(
            f"[{i}:v]scale={TILE_W}:{TILE_H}:force_original_aspect_ratio=decrease,"
            f"pad={TILE_W}:{TILE_H}:-1:-1:color={TILE_BG},setsar=1[v{i}]")
    # xstack layout: uniform tiles, so positions are multiples of w0/h0.
    layout = []
    for i in range(n):
        c, r = i % cols, i // cols
        x = "0" if c == 0 else "+".join(["w0"] * c)
        y = "0" if r == 0 else "+".join(["h0"] * r)
        layout.append(f"{x}_{y}")
    chain = ";".join(filters)
    refs = "".join(f"[v{i}]" for i in range(n))
    if n == 1:
        fc = f"{chain};[v0]copy[out]"
    else:
        fc = f"{chain};{refs}xstack=inputs={n}:layout={'|'.join(layout)}:fill={TILE_BG}[out]"
    run(["ffmpeg", "-y", *inputs, "-filter_complex", fc, "-map", "[out]",
         "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "20",
         "-movflags", "+faststart", out_mp4],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def slugify(s):
    return re.sub(r"[^a-z0-9]+", "-", s.lower()).strip("-") or "scenario"


def publish(info, frames_dir, content_root, note=None):
    stamp = datetime.now(timezone.utc).strftime("%Y-%m-%d_%H-%M-%S")
    folder = os.path.join(content_root, f"{stamp}.1")
    os.makedirs(folder, exist_ok=True)
    fps = info["fps"]

    print("==> Encoding videos")
    composite = os.path.join(folder, "all_cameras.mp4")
    encode_composite(frames_dir, info["cameras"], fps, composite)

    cam_files = []
    for name, w, h in info["cameras"]:
        mp4 = os.path.join(folder, f"{name}.mp4")
        png = os.path.join(folder, f"{name}.png")
        encode_camera(frames_dir, name, fps, mp4)
        encode_poster(frames_dir, name, png)
        cam_files.append((name, w, h))

    duration = info["frames"] / fps if fps else 0
    write_index(folder, info, cam_files, duration, note)
    write_metadata(folder, info, duration)

    # Feature it under Active/.
    active = os.path.join(content_root, "Active")
    os.makedirs(active, exist_ok=True)
    link = os.path.join(active, slugify(info["name"]))
    if os.path.islink(link) or os.path.exists(link):
        os.remove(link)
    os.symlink(os.path.join("..", f"{stamp}.1"), link)
    print(f"==> Published to {folder}")
    print(f"    Featured at Active/{slugify(info['name'])}")
    return folder


def write_metadata(folder, info, duration):
    with open(os.path.join(folder, "metadata.yaml"), "w") as f:
        f.write(f"title: 'MAD: {info['name']}'\n")
        f.write("kind: gameplay-capture\n")
        f.write(f"cameras: {len(info['cameras'])}\n")
        f.write(f"frames: {info['frames']}\n")
        f.write(f"duration_seconds: {duration:.1f}\n")
        f.write(f"demons_reached_nexus: {info['reached_nexus']}\n")


def write_index(folder, info, cam_files, duration, note=None):
    legend = "".join(
        f'<span class="chip"><i style="background:{c}"></i>{n}</span>'
        for n, c in DEMON_LEGEND)
    note_html = f'<section class="note">{note}</section>' if note else ""
    cams_html = []
    for name, w, h in cam_files:
        cams_html.append(f"""
      <figure>
        <video controls loop muted playsinline poster="{name}.png" preload="none">
          <source src="{name}.mp4" type="video/mp4">
        </video>
        <figcaption>{name} &middot; {w}&times;{h}</figcaption>
      </figure>""")
    html = f"""<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>MAD: {info['name']}</title>
<style>
  :root {{ color-scheme: dark; }}
  body {{ margin:0; background:#0c0a14; color:#e8e4f0;
         font:15px/1.5 -apple-system,Segoe UI,Roboto,sans-serif; }}
  header {{ padding:24px 28px; border-bottom:1px solid #232036;
            background:#13101f; }}
  h1 {{ margin:0 0 6px; font-size:22px; }}
  .stats {{ color:#a59fc0; font-size:14px; }}
  .stats b {{ color:#f0d27a; }}
  main {{ padding:24px 28px; max-width:1200px; margin:0 auto; }}
  section h2 {{ font-size:15px; text-transform:uppercase; letter-spacing:.08em;
                color:#8e88ac; margin:28px 0 12px; }}
  video {{ width:100%; border-radius:10px; background:#000; display:block; }}
  .hero video {{ border:1px solid #2a2640; }}
  .grid {{ display:grid; grid-template-columns:repeat(auto-fit,minmax(280px,1fr));
           gap:18px; }}
  figure {{ margin:0; }}
  figcaption {{ color:#8e88ac; font-size:13px; margin-top:6px; text-align:center; }}
  .legend {{ display:flex; gap:14px; flex-wrap:wrap; margin-top:10px; }}
  .chip {{ display:inline-flex; align-items:center; gap:6px; font-size:13px;
           color:#c8c2e0; }}
  .chip i {{ width:12px; height:12px; border-radius:3px; display:inline-block; }}
  .note {{ background:#15121f; border:1px solid #2a2640; border-radius:10px;
           padding:18px 22px; margin-bottom:8px; }}
  .note h3 {{ margin:0 0 8px; font-size:16px; }}
  .note p {{ margin:8px 0; color:#c8c2e0; }}
  .note .sw {{ display:inline-block; width:14px; height:14px; border-radius:3px;
               vertical-align:middle; margin-right:6px; }}
</style></head>
<body>
<header>
  <h1>MAD &middot; {info['name']}</h1>
  <div class="stats">
    {len(cam_files)} cameras &middot; {info['frames']} frames &middot;
    {duration:.0f}s &middot;
    <b>{info['reached_nexus']}</b> demons reached the Nexus
  </div>
  <div class="legend">{legend}</div>
</header>
<main>
  {note_html}
  <section class="hero">
    <h2>All cameras</h2>
    <video controls autoplay loop muted playsinline>
      <source src="all_cameras.mp4" type="video/mp4">
    </video>
  </section>
  <section>
    <h2>Per-camera</h2>
    <div class="grid">{''.join(cams_html)}</div>
  </section>
</main>
</body></html>"""
    with open(os.path.join(folder, "index.html"), "w") as f:
        f.write(html)


def main():
    ap = argparse.ArgumentParser(description="MAD AI capture pipeline")
    ap.add_argument("scenario", help="path to a .mad scenario file")
    ap.add_argument("--no-build", action="store_true", help="skip the Release build")
    ap.add_argument("--no-publish", action="store_true",
                    help="record + encode but do not publish to /Content")
    ap.add_argument("--frames-dir", default=None,
                    help="scratch dir for PPM frames (default under /Data)")
    ap.add_argument("--content-root", default="/Content")
    ap.add_argument("--note", default=None,
                    help="HTML snippet shown above the videos on the report page")
    ap.add_argument("--note-file", default=None,
                    help="read the --note HTML from a file")
    ap.add_argument("--trails", action="store_true",
                    help="draw demon path trails (great for showing pathing)")
    args = ap.parse_args()

    scenario = os.path.abspath(args.scenario)
    if not os.path.exists(scenario):
        sys.exit(f"scenario not found: {scenario}")
    name = os.path.splitext(os.path.basename(scenario))[0]
    frames_dir = args.frames_dir or f"/Data/mad_capture/{name}"

    if not args.no_build:
        build_release()
    record(scenario, frames_dir, trails=args.trails)
    info = parse_manifest(frames_dir)
    print(f"==> {info['name']}: {len(info['cameras'])} cameras, "
          f"{info['frames']} frames, {info['reached_nexus']} reached nexus")

    if args.no_publish:
        print("==> Skipping publish (--no-publish)")
        return
    note = args.note
    if args.note_file:
        with open(args.note_file) as f:
            note = f.read()
    folder = publish(info, frames_dir, args.content_root, note)
    print(f"\nDone. Open {os.path.join(folder, 'index.html')}")


if __name__ == "__main__":
    main()
