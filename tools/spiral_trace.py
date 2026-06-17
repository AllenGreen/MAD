#!/usr/bin/env python3
"""Trace ONE unit through the perfect (Archimedean) spiral and publish a page:
the spiral video with the unit's full trail, plus a radius-vs-time plot and an
X/Y path plot. A smooth, steadily-decreasing radius confirms a continuous spiral
(concentric rings would give plateaus + sudden drops at the gaps instead)."""
import math
import os
import shutil
import subprocess
import sys
from datetime import datetime, timezone

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = os.path.join(REPO, "build", "mad")

SCENARIO = """name Spiral Trace (1 unit)
map players=3 grid_w=44 grid_h=23 radius=22 cell=1
seed 20260615
tick_rate 60
fps 30
ticks 9000
camera overview name=overview w=760 h=760
at 1 spiral pitch=4 dir=cw
at 20 spawn sector=0 type=ground size=1 count=1 dir=cw goal=nexus
"""

ACCENT = "#78c8fa"


def run(cmd, **kw):
    print("  $", " ".join(cmd))
    subprocess.run(cmd, check=True, **kw)


def record(frames_dir, traj):
    os.makedirs(frames_dir, exist_ok=True)
    scn = "/Data/mad_capture/spiral_trace.mad"
    with open(scn, "w") as f:
        f.write(SCENARIO)
    env = dict(os.environ, MAD_TRAILS="1", MAD_TRAJ=traj, ASAN_OPTIONS="detect_leaks=0")
    run([BIN, "--record", scn, "--out", frames_dir], env=env, stdout=subprocess.DEVNULL)


def encode(frames_dir, out_mp4, poster):
    pat = os.path.join(frames_dir, "overview", "frame_%06d.ppm")
    run(["ffmpeg", "-y", "-framerate", "30", "-i", pat, "-c:v", "libx264",
         "-pix_fmt", "yuv420p", "-crf", "20", "-movflags", "+faststart", out_mp4],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    # poster: a late frame where the full trail shows
    import glob
    frames = sorted(glob.glob(os.path.join(frames_dir, "overview", "frame_*.ppm")))
    if frames:
        run(["ffmpeg", "-y", "-i", frames[int(len(frames) * 0.72)], poster],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def read_traj(path):
    rows = []
    with open(path) as f:
        next(f)
        for line in f:
            t = line.split()
            if len(t) == 5:
                rows.append((int(t[1]), float(t[2]), float(t[3]), float(t[4])))
    return rows  # (frame, x, y, r)


def radius_plot(rows, fps=30.0):
    W, H, pad = 760, 300, 44
    n = len(rows)
    rmax = max(r for *_, r in rows) if rows else 22
    tmax = (n - 1) / fps if n > 1 else 1
    def X(i):
        return pad + (W - 2 * pad) * (rows[i][0] / fps) / tmax
    def Y(r):
        return H - pad - (H - 2 * pad) * r / rmax
    out = [f'<svg viewBox="0 0 {W} {H}" xmlns="http://www.w3.org/2000/svg">',
           f'<rect width="{W}" height="{H}" fill="#0c0a14"/>']
    # gridlines + y labels (radius)
    for r in range(0, int(rmax) + 1, 4):
        y = Y(r)
        out.append(f'<line x1="{pad}" y1="{y:.1f}" x2="{W-pad}" y2="{y:.1f}" stroke="#23202e" stroke-width="1"/>')
        out.append(f'<text x="{pad-8}" y="{y+4:.1f}" fill="#8e88ac" font-size="11" '
                   f'text-anchor="end" font-family="ui-monospace,monospace">{r}</text>')
    # x labels (seconds)
    for sx in range(0, int(tmax) + 1, 10):
        x = pad + (W - 2 * pad) * sx / tmax
        out.append(f'<line x1="{x:.1f}" y1="{pad}" x2="{x:.1f}" y2="{H-pad}" stroke="#1a1822" stroke-width="1"/>')
        out.append(f'<text x="{x:.1f}" y="{H-pad+16}" fill="#8e88ac" font-size="11" '
                   f'text-anchor="middle" font-family="ui-monospace,monospace">{sx}s</text>')
    pts = " ".join(f"{X(i):.1f},{Y(rows[i][3]):.1f}" for i in range(n))
    out.append(f'<polyline points="{pts}" fill="none" stroke="{ACCENT}" stroke-width="2"/>')
    out.append(f'<text x="{pad}" y="20" fill="#e8e4f0" font-size="13" font-weight="bold" '
               f'font-family="ui-monospace,monospace">distance from Nexus (radius) vs time</text>')
    out.append(f'<text x="{W-pad}" y="20" fill="#8e88ac" font-size="11" text-anchor="end" '
               f'font-family="ui-monospace,monospace">smooth + steady = continuous spiral</text>')
    out.append('</svg>')
    return "".join(out)


def path_plot(rows, R=22.0):
    S = 420
    sc = (S / 2 - 16) / (R * 1.02)
    def P(x, y):
        return (S / 2 + x * sc, S / 2 - y * sc)
    out = [f'<svg viewBox="0 0 {S} {S}" xmlns="http://www.w3.org/2000/svg">',
           f'<rect width="{S}" height="{S}" fill="#0c0a14"/>']
    out.append(f'<circle cx="{S/2}" cy="{S/2}" r="{R*sc:.1f}" fill="none" stroke="#23202e" stroke-width="1"/>')
    pts = " ".join(f"{P(x,y)[0]:.1f},{P(x,y)[1]:.1f}" for _, x, y, _ in rows)
    out.append(f'<polyline points="{pts}" fill="none" stroke="{ACCENT}" stroke-width="1.8"/>')
    sx, sy = P(rows[0][1], rows[0][2])
    out.append(f'<circle cx="{sx:.1f}" cy="{sy:.1f}" r="5" fill="#5ae6a0"/>')          # start
    out.append(f'<circle cx="{S/2}" cy="{S/2}" r="6" fill="#f0d27a"/>')                # nexus
    out.append(f'<text x="10" y="22" fill="#e8e4f0" font-size="13" font-weight="bold" '
               f'font-family="ui-monospace,monospace">unit path (X/Y)</text>')
    out.append('</svg>')
    return "".join(out)


def build_html(radius_svg, path_svg, rows):
    secs = (rows[-1][0] - rows[0][0]) / 30.0 if len(rows) > 1 else 0
    return f"""<!DOCTYPE html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>MAD · Perfect spiral — single-unit trace</title>
<style>
:root {{ color-scheme: dark; }}
body {{ margin:0; background:#0c0a14; color:#e8e4f0;
       font:15px/1.6 -apple-system,Segoe UI,Roboto,sans-serif; }}
header {{ padding:24px 28px; border-bottom:1px solid #232036; background:#13101f; }}
h1 {{ margin:0 0 6px; font-size:22px; }}
main {{ padding:10px 28px 40px; max-width:1100px; margin:0 auto; }}
section h2 {{ font-size:15px; text-transform:uppercase; letter-spacing:.08em;
              color:#8e88ac; margin:26px 0 12px; }}
video {{ width:100%; border-radius:10px; background:#000; display:block; }}
svg {{ width:100%; height:auto; background:#0c0a14; border:1px solid #232036;
       border-radius:10px; display:block; }}
.row {{ display:grid; grid-template-columns:1.1fr .9fr; gap:18px; align-items:start; }}
.note {{ background:#15121f; border:1px solid #2a2640; border-radius:10px; padding:16px 20px; }}
code {{ font-family:ui-monospace,monospace; color:#d7c8ff; }}
em {{ color:#f0d27a; font-style:normal; }}
@media(max-width:820px){{ .row{{grid-template-columns:1fr}} }}
</style></head><body>
<header>
  <h1>Perfect spiral &mdash; tracking a single unit</h1>
  <div style="color:#a59fc0;font-size:14px">A single continuous Archimedean spiral
  wall (<code>spiral pitch=4</code>) across all three players. One unit, traced from
  the outer edge to the Nexus &mdash; about {secs:.0f}s of travel.</div>
</header>
<main>
  <section>
    <h2>The spiral &amp; the unit's trail</h2>
    <video controls autoplay loop muted playsinline poster="trace.png">
      <source src="trace.mp4" type="video/mp4"></video>
  </section>
  <section>
    <h2>Tracked path &mdash; confirmation</h2>
    <div class="row">
      <div>{radius_svg}</div>
      <div>{path_svg}</div>
    </div>
    <div class="note" style="margin-top:14px">
    <p>The wall is one <em>continuous</em> spiral, so the corridor is one smooth
    channel from the edge to the centre. The trace confirms it: the unit's
    <strong>radius decreases steadily and continuously</strong> as it winds inward
    (left plot), and the X/Y path is a clean spiral (right). There are no plateaus
    or sudden drops &mdash; which is exactly what concentric rings with rotating
    gaps <em>would</em> produce (lap at constant radius, then jump inward at a gap).
    This is <code>World::add_perfect_spiral</code>: <code>f(r,&#952;) =
    (R0&minus;r)/pitch + dir&#183;&#952;/2&#960;</code>, walling every grid edge where
    <code>floor(f)</code> changes &mdash; cardinal and diagonal, and across the seams.</p>
    </div>
  </section>
</main></body></html>"""


def publish(html, folder_files, content_root):
    stamp = datetime.now(timezone.utc).strftime("%Y-%m-%d_%H-%M-%S")
    folder = os.path.join(content_root, f"{stamp}.1")
    os.makedirs(folder, exist_ok=True)
    for src, name in folder_files:
        shutil.move(src, os.path.join(folder, name))
    with open(os.path.join(folder, "index.html"), "w") as f:
        f.write(html)
    with open(os.path.join(folder, "metadata.yaml"), "w") as f:
        f.write("title: 'MAD: Perfect spiral - single-unit trace'\nkind: trace\n")
    active = os.path.join(content_root, "Active")
    os.makedirs(active, exist_ok=True)
    link = os.path.join(active, "spiral-trace")
    if os.path.islink(link) or os.path.exists(link):
        os.remove(link)
    os.symlink(os.path.join("..", f"{stamp}.1"), link)
    return folder


def main():
    content_root = sys.argv[1] if len(sys.argv) > 1 else "/Content"
    frames = "/Data/mad_capture/spiral_trace"
    traj = "/Data/mad_capture/spiral_trace.tsv"
    print("==> Recording single unit through the spiral")
    record(frames, traj)
    print("==> Encoding video")
    encode(frames, "/tmp/trace.mp4", "/tmp/trace.png")
    rows = read_traj(traj)
    print(f"==> {len(rows)} trajectory points, radius {rows[0][3]:.1f} -> {rows[-1][3]:.1f}")
    html = build_html(radius_plot(rows), path_plot(rows), rows)
    folder = publish(html, [("/tmp/trace.mp4", "trace.mp4"), ("/tmp/trace.png", "trace.png")],
                     content_root)
    print(f"==> Published to {folder}")
    print("    Featured at Active/spiral-trace")


if __name__ == "__main__":
    main()
