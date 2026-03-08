"""Quick validation of all pattern generators."""
import os, sys
os.chdir(os.path.dirname(os.path.abspath(__file__)))
import matplotlib
matplotlib.use("Agg")
import DemoStringArts as SA
import matplotlib.pyplot as plt
plt.show = lambda: None

for idx in range(len(SA.PATTERNS)):
    name, gen_func = SA.PATTERNS[idx]
    path = gen_func(SA.NUM_PEGS)
    tokens = SA.build_commands(path, SA.NUM_PEGS)
    assert len(tokens) % 4 == 0
    for i in range(0, len(tokens), 4):
        slot = int(tokens[i])
        wrap = int(tokens[i+2])
        assert tokens[i+1] == "D" and tokens[i+3] == "U"
        assert (wrap - slot) % SA.NUM_PEGS == 1, \
            f"{name}: wrap {wrap} is not slot+1 from {slot}"
    print(f"[{idx+1}] {name}: {len(tokens)//4} wraps, always +1 OK")

print("\nAll patterns passed — wrap is always +1!")
