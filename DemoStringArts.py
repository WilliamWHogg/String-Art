"""
Geometric String Art Pattern Generator
========================================
Generates string art command files for the ESP32 String Art Controller.
Shows a preview of the pattern and exports a CSV file ready to upload.

Command format:  slot,D,slot+1,U,slot,D,slot+1,U,...
  - The turntable moves to a peg (while threader is up), the threader goes
    Down, the turntable nudges +1 slot to wrap the thread around the peg,
    then the threader goes Up.  The turntable then travels to the next peg.
  - The string is continuous: each line starts where the last wrap ended.

Usage:
    1. Edit the CONFIGURATION and PATTERN PARAMETERS sections below
    2. Set SELECTED_PATTERNS to the pattern numbers you want (or [0] for all)
    3. Run:  python DemoStringArts.py
    4. A preview window opens and .txt files are saved in the same folder
    5. Upload the .txt files to the ESP32 web interface

Output files can be uploaded directly through the ESP32 web interface.
"""

import math
import os
import matplotlib
matplotlib.use("TkAgg")
import matplotlib.pyplot as plt
import matplotlib.collections as mc
import numpy as np


# ── CONFIG ────────────────────────────────────────────────────────────────────
NUM_PEGS          = 200          # pegs on turntable (match ESP32 config)
STRING_COLOR      = "white"      # "white" on black bg, or "black" on white bg
MAX_THREADS       = 5000         # max connections per pattern
OUTPUT_DIR        = os.path.dirname(os.path.abspath(__file__))
SELECTED_PATTERNS = [1]          # [0]=all, [1]=Cardioid, [2]=NestedPoly

# ── PATTERN PARAMS ────────────────────────────────────────────────────────────
CARDIOID_MULTIPLIER = 2          # 2=cardioid, 3=nephroid, 4=3-cusp
NEST_SIDES          = 3          # polygon sides (3=tri, 4=sq, 6=hex)
NEST_LAYERS         = 3          # concentric rings
NEST_ROTATION       = 20         # degrees rotated per layer

# ─────────────────────────────────────────────────────────────────────────────

def peg_pos(slot, num_pegs):
    """Return (x, y) of a peg on a unit circle."""
    angle = 2 * math.pi * slot / num_pegs
    return (math.cos(angle), math.sin(angle))


def _clean_path(path):
    """Remove consecutive duplicate pegs from a path."""
    if not path:
        return path
    clean = [path[0]]
    for peg in path[1:]:
        if peg != clean[-1]:
            clean.append(peg)
    return clean


def build_commands(path, num_pegs):
    """
    Convert a peg path into ESP32 command tokens.

    The thread is continuous: after wrapping peg A (ending at A+1),
    the turntable moves to the next peg B.  The visible string line
    runs from A+1 to B.

    Physical sequence at each peg:
      1. Turntable moves to the slot           (threader is UP)
      2. Threader goes DOWN                     (needle enters the peg gap)
      3. Turntable nudges +1 slot              (wraps thread around the peg)
      4. Threader goes UP                       (needle exits)

    Output format:  slot,D,slot+1,U, ... ,next_slot,D,next_slot+1,U, ...
    """
    path = _clean_path(path)
    if len(path) < 2:
        return []

    tokens = []
    for peg in path:
        wrap = (peg + 1) % num_pegs
        tokens.extend([str(peg), "D", str(wrap), "U"])
    return tokens


def save_commands(tokens, filename):
    """Save command tokens to a CSV text file."""
    path = os.path.join(OUTPUT_DIR, filename)
    with open(path, "w") as f:
        f.write(",".join(tokens))
    threads = len(tokens) // 4
    print(f"  Saved {threads} peg visits ({len(tokens)} commands) -> {path}")


def preview(path, num_pegs, title, string_color="white"):
    """Show a matplotlib preview of the actual thread path."""
    path = _clean_path(path)
    bg = "black" if string_color == "white" else "white"
    line_color = string_color

    # Compute actual thread lines (from each wrap position to next peg)
    thread_lines = []
    prev_wrap = None
    for peg in path:
        wrap = (peg + 1) % num_pegs
        if prev_wrap is not None:
            p1 = peg_pos(prev_wrap, num_pegs)
            p2 = peg_pos(peg, num_pegs)
            thread_lines.append([p1, p2])
        prev_wrap = wrap

    alpha = max(0.03, min(0.4, 80.0 / max(len(thread_lines), 1)))

    fig, ax = plt.subplots(1, 1, figsize=(8, 8), facecolor=bg)
    ax.set_facecolor(bg)
    ax.set_aspect("equal")
    ax.set_title(title, color=line_color, fontsize=14, fontweight="bold", pad=12)
    ax.axis("off")

    # Draw circle of pegs
    peg_angles = np.linspace(0, 2 * np.pi, num_pegs, endpoint=False)
    px = np.cos(peg_angles)
    py = np.sin(peg_angles)
    ax.scatter(px, py, s=4, c="gray", zorder=3)

    # Draw thread
    lc = mc.LineCollection(thread_lines, colors=line_color, linewidths=0.4, alpha=alpha)
    ax.add_collection(lc)

    ax.set_xlim(-1.15, 1.15)
    ax.set_ylim(-1.15, 1.15)
    plt.tight_layout()
    return fig


# ─── Pattern Generators ─────────────────────────────────────────────────────
# Each generator returns an ordered list of peg indices (the thread path).
# The turntable moves to each peg in sequence (shortest path, CW or CCW).

def pattern_cardioid(num_pegs, multiplier=2):
    """
    Cardioid / nephroid: connect peg i to peg (i * multiplier) mod N.
    Walks i = 0, 1, 2, ... visiting each cardioid chord in order.
    """
    path = []
    visited = set()
    for i in range(num_pegs):
        j = (i * multiplier) % num_pegs
        if i == j:
            continue
        pair = (min(i, j), max(i, j))
        if pair in visited:
            continue
        visited.add(pair)
        path.append(i)
        path.append(j)
    return path[:MAX_THREADS]


def pattern_nested_polygons(num_pegs, sides=6, layers=12, rotation_step=7.5):
    """Concentric rotated polygons — vertices traced in order per layer."""
    path = []
    for layer in range(layers):
        offset = int(layer * rotation_step / 360.0 * num_pegs) % num_pegs
        step = max(1, num_pegs // sides)
        for i in range(sides):
            peg = (i * step + offset) % num_pegs
            path.append(peg)
        # Close polygon back to first vertex of this layer
        path.append((0 * step + offset) % num_pegs)
    return path[:MAX_THREADS]


# ─── Pattern Registry ───────────────────────────────────────────────────────

PATTERNS = [
    ("Cardioid",          lambda n: pattern_cardioid(n, multiplier=CARDIOID_MULTIPLIER)),
    ("Nested Polygons",   lambda n: pattern_nested_polygons(n, sides=NEST_SIDES, layers=NEST_LAYERS, rotation_step=NEST_ROTATION)),
]


# ─── Main ───────────────────────────────────────────────────────────────────

def main():
    print(f"Geometric String Art Generator -- {NUM_PEGS} pegs, {STRING_COLOR} string")
    print(f"{'='*60}")

    # Resolve selected patterns from config
    if 0 in SELECTED_PATTERNS:
        selected = list(range(len(PATTERNS)))
        print("Generating ALL patterns...")
    else:
        selected = [p - 1 for p in SELECTED_PATTERNS if 1 <= p <= len(PATTERNS)]
        if not selected:
            print("No valid patterns in SELECTED_PATTERNS. Set to [0] for all.")
            return
        names = [PATTERNS[i][0] for i in selected]
        print(f"Generating: {', '.join(names)}")

    figs = []
    for idx in selected:
        name, gen_func = PATTERNS[idx]
        print(f"\n[{idx+1}] {name}")

        path = gen_func(NUM_PEGS)
        print(f"  {len(path)} peg visits ({len(path) - 1} thread lines)")

        tokens = build_commands(path, NUM_PEGS)
        filename = f"pattern_{name.lower().replace(' ', '_').replace('x', 'x')}.txt"
        save_commands(tokens, filename)

        fig = preview(path, NUM_PEGS, f"{name}  ({len(path) - 1} thread lines)",
                      string_color=STRING_COLOR)
        figs.append(fig)

    print(f"\n{'='*60}")
    print("Done! Upload the .txt files to the ESP32 web interface.")
    print("Showing previews...")
    plt.show()


if __name__ == "__main__":
    main()
