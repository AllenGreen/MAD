#!/usr/bin/env python3
"""Render a content page explaining MAD's sector coordinate system and the
cross-sector border ("seam"), with zoomed SVG diagrams at three radii.

The geometry comes straight from the engine (`mad --coords`), so the diagrams
provably match the code. Publishes to /Content.
"""
import math
import os
import subprocess
import sys
from datetime import datetime, timezone

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ---- world -> SVG transform (shared scale across views for comparison) --------
WIN = 4.6           # world-unit half-window around the seam point
SVG = 600           # drawing square (px)
SCALE = (SVG / 2 - 12) / (WIN + 0.4)
CXC = SVG / 2

C_A = "#34507a"      # sector 0 cells (blue)
C_B = "#7a5436"      # sector 1 cells (amber)
C_MASK = "#23232e"   # masked / out-of-wedge cells
C_SEAM = "#ff5a5a"
C_RING = "#66cc88"
C_CROSS = "#5ae6a0"
C_NEXUS = "#f0d27a"


def parse(path):
    data = {"config": {}, "seam": {}, "views": []}
    view = None
    with open(path) as f:
        for line in f:
            t = line.split()
            if not t:
                continue
            kind = t[0]
            kv = dict(p.split("=", 1) for p in t[1:] if "=" in p)
            if kind == "config":
                data["config"] = {k: float(v) for k, v in kv.items()}
            elif kind == "seam":
                data["seam"] = {k: float(v) for k, v in kv.items()
                                if k.endswith("deg")} | {"a": int(kv["a"]), "b": int(kv["b"])}
            elif kind == "view":
                # label may contain spaces: reconstruct from the raw line
                label = line.split("label=", 1)[1].split(" radius=")[0]
                sp = kv["seampoint"].split(",")
                view = {"label": label, "radius": float(kv["radius"]),
                        "seam": (float(sp[0]), float(sp[1])), "cells": [], "cross": None}
                data["views"].append(view)
            elif kind == "cell":
                cor = [tuple(map(float, p.split(","))) for p in kv["corners"].split(";")]
                view["cells"].append({
                    "sect": int(kv["sect"]), "col": int(kv["col"]), "row": int(kv["row"]),
                    "c": (float(kv["cx"]), float(kv["cy"])), "r": float(kv["r"]),
                    "walk": kv["walk"] == "1", "corners": cor})
            elif kind == "cross":
                def pt(s):
                    return tuple(map(float, kv[s].split(",")))
                def cell(s):
                    return tuple(map(int, kv[s].split(",")))
                view["cross"] = {"sample": pt("sample"), "nudgeA": pt("nudgeA"),
                                 "nudgeB": pt("nudgeB"), "cellA": cell("cellA"),
                                 "cellB": cell("cellB"), "centerA": pt("centerA"),
                                 "centerB": pt("centerB")}
    return data


def tf(p, seam):
    """world -> svg pixel, centred on the view's seam point (+Y up -> -y)."""
    return (CXC + (p[0] - seam[0]) * SCALE, CXC - (p[1] - seam[1]) * SCALE)


def poly(corners, seam, fill, stroke, sw=1.0, op=1.0):
    pts = " ".join(f"{x:.1f},{y:.1f}" for x, y in (tf(c, seam) for c in corners))
    return (f'<polygon points="{pts}" fill="{fill}" fill-opacity="{op}" '
            f'stroke="{stroke}" stroke-width="{sw}"/>')


def line(p0, p1, seam, stroke, sw, dash="", marker=""):
    a, b = tf(p0, seam), tf(p1, seam)
    d = f'stroke-dasharray="{dash}" ' if dash else ""
    m = f'marker-end="url(#{marker})" ' if marker else ""
    return (f'<line x1="{a[0]:.1f}" y1="{a[1]:.1f}" x2="{b[0]:.1f}" y2="{b[1]:.1f}" '
            f'stroke="{stroke}" stroke-width="{sw}" {d}{m}/>')


def dot(p, seam, r, fill, stroke="none", sw=0):
    x, y = tf(p, seam)
    return f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"/>'


def text(p, seam, s, fill, size=11, anchor="middle", dy=4, weight="normal"):
    x, y = tf(p, seam)
    return (f'<text x="{x:.1f}" y="{y + dy:.1f}" fill="{fill}" font-size="{size}" '
            f'text-anchor="{anchor}" font-weight="{weight}" '
            f'font-family="ui-monospace,monospace">{s}</text>')


def context_inset(config, seam_angle_deg, radius):
    """Small 3-wedge pie in the corner marking where this zoom is."""
    n = int(config["players"])
    R = config["radius"]
    cx, cy, rad = SVG - 66, 66, 46
    scale = rad / R
    out = [f'<g>', f'<circle cx="{cx}" cy="{cy}" r="{rad}" fill="#15121f" stroke="#2a2640"/>']
    # seam lines (radial) at (i+0.5)*360/n
    for i in range(n):
        ang = math.radians((i + 0.5) * 360.0 / n)
        x = cx + math.sin(ang) * rad
        y = cy - math.cos(ang) * rad
        out.append(f'<line x1="{cx}" y1="{cy}" x2="{x:.1f}" y2="{y:.1f}" stroke="#3a3650" stroke-width="1"/>')
    # marker at this zoom (radius along the 0/1 seam)
    a = math.radians(seam_angle_deg)
    mx = cx + math.sin(a) * radius * scale
    my = cy - math.cos(a) * radius * scale
    out.append(f'<circle cx="{cx}" cy="{cy}" r="2.5" fill="{C_NEXUS}"/>')   # nexus
    out.append(f'<circle cx="{mx:.1f}" cy="{my:.1f}" r="4" fill="{C_SEAM}" stroke="#fff" stroke-width="1"/>')
    out.append(f'<text x="{cx}" y="{cy + rad + 13}" fill="#8e88ac" font-size="10" '
               f'text-anchor="middle" font-family="ui-monospace,monospace">whole map</text>')
    out.append('</g>')
    return "".join(out)


def render_view(view, config, seam):
    s = view["seam"]
    phi = math.radians(seam["angle_deg"])
    parts = [f'<svg viewBox="0 0 {SVG} {SVG}" xmlns="http://www.w3.org/2000/svg">']
    parts.append('<defs>'
                 f'<marker id="ah" markerWidth="9" markerHeight="9" refX="7" refY="3" orient="auto">'
                 f'<path d="M0,0 L7,3 L0,6 Z" fill="{C_CROSS}"/></marker>'
                 f'<marker id="ahn" markerWidth="9" markerHeight="9" refX="7" refY="3" orient="auto">'
                 f'<path d="M0,0 L7,3 L0,6 Z" fill="{C_NEXUS}"/></marker>'
                 '</defs>')
    parts.append(f'<rect width="{SVG}" height="{SVG}" fill="#0c0a14"/>')

    # constant-radius arc through the seam point (the "ring" a unit laps along).
    d = view["radius"]
    half = (WIN + 0.4) / d * 1.25
    arc = []
    steps = 48
    for k in range(steps + 1):
        a = phi - half + 2 * half * k / steps
        arc.append(tf((d * math.sin(a), d * math.cos(a)), s))
    pts = " ".join(f"{x:.1f},{y:.1f}" for x, y in arc)
    parts.append(f'<polyline points="{pts}" fill="none" stroke="{C_RING}" '
                 f'stroke-width="1.4" stroke-dasharray="5 4" opacity="0.8"/>')

    # cells (masked first, then walkable, then labels)
    for cell in sorted(view["cells"], key=lambda c: c["walk"]):
        col = (C_A if cell["sect"] == 0 else C_B) if cell["walk"] else C_MASK
        op = 1.0 if cell["walk"] else 0.22  # masked grid extensions: very faint
        stroke = "#26304a" if cell["walk"] else "#1a1a22"
        parts.append(poly(cell["corners"], s, col, stroke, 1.0, op))
    for cell in view["cells"]:
        if cell["walk"]:
            parts.append(text(cell["c"], s, f'{cell["col"]},{cell["row"]}',
                              "#c9d4e6", 10))

    # the seam line (radial, straight) across the view
    dir_seam = (math.sin(phi), math.cos(phi))
    big = (WIN + 2)
    p0 = (s[0] - dir_seam[0] * big, s[1] - dir_seam[1] * big)
    p1 = (s[0] + dir_seam[0] * big, s[1] + dir_seam[1] * big)
    parts.append(line(p0, p1, s, C_SEAM, 3, "8 5"))

    # toward-nexus arrow (down the radial)
    npt = (s[0] - dir_seam[0] * 1.7, s[1] - dir_seam[1] * 1.7)
    parts.append(line(s, npt, s, C_NEXUS, 2, marker="ahn"))
    parts.append(text(npt, s, "to Nexus", C_NEXUS, 11, dy=-6))

    cr = view["cross"]
    if cr:
        # nudge: sample on seam, +/-0.7 perpendicular into each sector
        parts.append(line(cr["sample"], cr["nudgeA"], s, "#9aa6c0", 1, "2 2"))
        parts.append(line(cr["sample"], cr["nudgeB"], s, "#9aa6c0", 1, "2 2"))
        parts.append(dot(cr["sample"], s, 4, "none", "#ffffff", 1.5))
        parts.append(dot(cr["nudgeA"], s, 2.5, "#9ad0ff"))
        parts.append(dot(cr["nudgeB"], s, 2.5, "#ffc89a"))
        # the crossing (handoff) between the two paired cells
        parts.append(line(cr["centerA"], cr["centerB"], s, C_CROSS, 2.5, marker="ah"))
        parts.append(line(cr["centerB"], cr["centerA"], s, C_CROSS, 2.5, marker="ah"))
        parts.append(dot(cr["centerA"], s, 4.5, C_CROSS, "#0c0a14", 1))
        parts.append(dot(cr["centerB"], s, 4.5, C_CROSS, "#0c0a14", 1))
        mid = ((cr["centerA"][0] + cr["centerB"][0]) / 2,
               (cr["centerA"][1] + cr["centerB"][1]) / 2)
        parts.append(text(mid, s, "crossing", C_CROSS, 11, dy=16, weight="bold"))

    parts.append(context_inset(config, seam["angle_deg"], view["radius"]))
    # title + seam-point readout
    parts.append(f'<text x="14" y="26" fill="#e8e4f0" font-size="17" font-weight="bold" '
                 f'font-family="ui-monospace,monospace">{view["label"]}</text>')
    parts.append(f'<text x="14" y="46" fill="#8e88ac" font-size="12" '
                 f'font-family="ui-monospace,monospace">radius ≈ {view["radius"]:.0f} '
                 f'· seam at {seam["angle_deg"]:.0f}° · '
                 f'A=s0 cell {cr["cellA"][0]},{cr["cellA"][1]} ↔ '
                 f'B=s1 cell {cr["cellB"][0]},{cr["cellB"][1]}</text>')
    parts.append('</svg>')
    return "".join(parts)


PAGE_CSS = """
:root { color-scheme: dark; }
body { margin:0; background:#0c0a14; color:#e8e4f0;
       font:15px/1.6 -apple-system,Segoe UI,Roboto,sans-serif; }
header { padding:24px 28px; border-bottom:1px solid #232036; background:#13101f; }
h1 { margin:0 0 6px; font-size:22px; }
main { padding:8px 28px 40px; max-width:1180px; margin:0 auto; }
section h2 { font-size:15px; text-transform:uppercase; letter-spacing:.08em;
             color:#8e88ac; margin:30px 0 12px; }
.views { display:grid; grid-template-columns:repeat(auto-fit,minmax(320px,1fr)); gap:18px; }
figure { margin:0; background:#13101f; border:1px solid #232036; border-radius:10px; padding:10px; }
figure svg { width:100%; height:auto; border-radius:6px; display:block; }
figcaption { color:#a59fc0; font-size:13px; margin-top:8px; }
.legend { display:flex; gap:16px; flex-wrap:wrap; margin:10px 0; }
.chip { display:inline-flex; align-items:center; gap:7px; font-size:13px; color:#c8c2e0; }
.chip i { width:14px; height:14px; border-radius:3px; display:inline-block; }
.note { background:#15121f; border:1px solid #2a2640; border-radius:10px; padding:16px 20px; }
code, .mono { font-family:ui-monospace,monospace; color:#d7c8ff; }
.formula { background:#0f0d18; border-left:3px solid #5a4a8a; padding:10px 14px;
           border-radius:6px; margin:10px 0; overflow-x:auto; font-family:ui-monospace,monospace;
           font-size:13.5px; color:#cdd6e6; white-space:pre; }
table { border-collapse:collapse; margin:10px 0; font-size:14px; }
td,th { border:1px solid #2a2640; padding:6px 12px; text-align:left; }
th { color:#a59fc0; }
em { color:#f0d27a; font-style:normal; }
"""


def legend():
    items = [(C_A, "Player 0 wedge (centre 0°)"),
             (C_B, "Player 1 wedge (centre 120°)"),
             (C_SEAM, "seam / border (a shared grid line)"),
             (C_RING, "constant radius (a row arc)"),
             (C_CROSS, "crossing: paired cells A↔B")]
    return '<div class="legend">' + "".join(
        f'<span class="chip"><i style="background:{c}"></i>{t}</span>' for c, t in items) + '</div>'


def build_html(data, svgs):
    cfg = data["config"]
    figs = []
    captions = [
        "Close to the Nexus the cells are short little arcs and narrow. Both players' "
        "grids meet the seam at the same rows (same radii) — the grid is continuous "
        "across the border.",
        "Half-way out. A row is a constant-radius arc; a column is a radial line. "
        "Player 0's last column abuts Player 1's column 0, row for row.",
        "At the extreme radius the cells are widest. The seam is just another grid "
        "line — a unit steps from one player's edge column straight into the "
        "neighbour's, no kink.",
    ]
    for v, svg, cap in zip(data["views"], svgs, captions):
        figs.append(f'<figure>{svg}<figcaption>{cap}</figcaption></figure>')
    views_html = "".join(figs)
    return f"""<!DOCTYPE html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>MAD · Sector coordinates & the border</title>
<style>{PAGE_CSS}</style></head><body>
<header>
  <h1>Sector coordinates &amp; the border (seam)</h1>
  <div style="color:#a59fc0;font-size:14px">Each player's grid is a <em>polar</em>
  wedge of one shared coordinate system, so the grid is a continuous (curved) line
  across every border. Geometry emitted by <code>mad --coords</code> (map:
  {int(cfg['players'])} players, {int(cfg['grid_w'])}×{int(cfg['grid_h'])} grid,
  radius {cfg['radius']:.0f}).</div>
</header>
<main>
  {legend()}
  <section>
    <h2>Zoomed views — near the Nexus, half-way, extreme radius</h2>
    <div class="views">{views_html}</div>
  </section>

  <section>
    <h2>1 &mdash; The polar grid</h2>
    <div class="note">
    <p>A player's grid is a <em>wedge of a polar grid</em>: a <strong>column maps to
    an angle</strong> across the wedge and a <strong>row maps to a radius</strong>.
    Columns are radial lines; rows are concentric arcs. Cells get narrower toward the
    Nexus.</p>
    <div class="formula">cell (col,row) &#8594; world (x,y), for player i of N
  half = &#960;/N                                  # half the wedge angle
  &#945; = (2&#960;i/N) &minus; half + (col+0.5)/W &#183; 2&#183;half     # angle from +Y, clockwise
  r = map_radius &#183; (1 &minus; (row+0.5)/H)             # row 0 = portal (outer), last row = Nexus
  x = r&#183;sin&#945;        y = r&#183;cos&#945;</div>
    <p>The inverse <code>world&#8594;cell</code> just reads the radius and angle back:
    <span class="mono">r = &#8730;(x&#178;+y&#178;)</span>,
    <span class="mono">&#945; = atan2(x,y)</span>, then bucket into (col,row). This is
    <code>Sector::cell_to_world</code> / <code>world_to_cell</code>
    (<code>src/game/sector.cpp</code>). In the diagrams Player&nbsp;0 is blue,
    Player&nbsp;1 amber; both are slightly curved cells.</p>
    </div>
  </section>

  <section>
    <h2>2 &mdash; The border is just a grid line</h2>
    <div class="note">
    <p>Because every player uses the same polar system, the border between player
    <span class="mono">i</span> and <span class="mono">i+1</span> is the radial line at
    <span class="mono">(i+0.5)&#183;2&#960;/N</span> (here
    <em>{data['seam']['angle_deg']:.0f}&#176;</em>) &mdash; which is <strong>exactly a
    column boundary</strong> of both grids. Player&nbsp;i's last column
    (<span class="mono">col = W&minus;1</span>) sits right against player&nbsp;i+1's first
    column (<span class="mono">col = 0</span>), and they share the same rows (radii).
    The grid line continues straight across the seam &mdash; no kink, no overlap.</p>
    </div>
  </section>

  <section>
    <h2>3 &mdash; Translating across the border</h2>
    <div class="note">
    <p>Pairing cells across the border is now trivial &mdash; same row, neighbouring
    column:</p>
    <div class="formula">cellA = (W&minus;1, row)  in player i      &#8596;      cellB = (0, row)  in player i+1
(both at radius r = map_radius &#183; (1 &minus; (row+0.5)/H))</div>
    <p>Every row gives a <em>crossing</em> (green) at the same radius, so units glide
    from one player's grid into the next with no jump, and a wall on one side meets the
    neighbour's wall continuously. The seam pairing
    (<code>GameMap::boundary_cells</code>) and the whole-map flow field
    (<code>GlobalFlowField</code>) use exactly this. A wall that crosses the seam is
    sealed at that radius to make the two players' walls one barrier
    (<code>World::add_boundary_wall</code>).</p>
    </div>
  </section>
</main></body></html>"""


def publish(html, svg_count, content_root):
    stamp = datetime.now(timezone.utc).strftime("%Y-%m-%d_%H-%M-%S")
    folder = os.path.join(content_root, f"{stamp}.1")
    os.makedirs(folder, exist_ok=True)
    with open(os.path.join(folder, "index.html"), "w") as f:
        f.write(html)
    with open(os.path.join(folder, "metadata.yaml"), "w") as f:
        f.write("title: 'MAD: Sector coordinates & the border'\n")
        f.write("kind: reference-diagram\n")
        f.write(f"views: {svg_count}\n")
    active = os.path.join(content_root, "Active")
    os.makedirs(active, exist_ok=True)
    link = os.path.join(active, "coordinate-system")
    if os.path.islink(link) or os.path.exists(link):
        os.remove(link)
    os.symlink(os.path.join("..", f"{stamp}.1"), link)
    return folder


def main():
    content_root = sys.argv[1] if len(sys.argv) > 1 else "/Content"
    dump = "/Data/mad_coords.txt"
    os.makedirs(os.path.dirname(dump), exist_ok=True)
    print("==> Dumping geometry from the engine")
    subprocess.run([os.path.join(REPO, "build", "mad"), "--coords", dump], check=True,
                   stdout=subprocess.DEVNULL)
    data = parse(dump)
    print(f"==> {len(data['views'])} views, seam at {data['seam']['angle_deg']:.0f} deg")
    svgs = [render_view(v, data["config"], data["seam"]) for v in data["views"]]
    html = build_html(data, svgs)
    folder = publish(html, len(svgs), content_root)
    print(f"==> Published to {folder}")
    print("    Featured at Active/coordinate-system")


if __name__ == "__main__":
    main()
