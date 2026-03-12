"""
test_accuracy.py — Generate an accuracy test command file.

Visits every slot in order 0..NUM_PEGS-1.
Even slots: threader DOWN (D)
Odd  slots: threader UP   (U)

This creates a simple alternating down/up sequence across all pegs so you
can visually verify that the turntable positions each peg correctly.

Upload the output file via the web interface, then press Start.
"""

NUM_PEGS = 200
DIRECTION = "CCW"   # "CW" = increasing slots (0→199), "CCW" = decreasing slots (0→1→199→198…)
OUTPUT_FILE = "test_accuracy_CCW.txt"

# Build the ordered slot list based on chosen direction
if DIRECTION.upper() == "CCW":
    slots = [0] + list(range(NUM_PEGS - 1, 0, -1))
else:
    slots = list(range(NUM_PEGS))

tokens = []
for i, slot in enumerate(slots):
    tokens.append(str(slot))
    tokens.append("D" if i % 2 == 0 else "U")

# Return threader up at the end if it finished down
if len(slots) % 2 == 1:
    tokens.append("U")

line = ",".join(tokens)

with open(OUTPUT_FILE, "w") as f:
    f.write(line)

print(f"Written {len(tokens)} tokens ({NUM_PEGS} slots, direction={DIRECTION}) to '{OUTPUT_FILE}'")
print("Preview:", line[:80], "...")
