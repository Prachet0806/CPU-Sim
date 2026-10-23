"""Sweep L1 cache capacity and plot the simulator's reported misses."""

import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    default_exe = ROOT / "build" / ("Release/cpu-sim.exe" if sys.platform == "win32" else "cpu-sim")
    parser.add_argument("--exe", type=Path, default=default_exe, help="simulator executable")
    parser.add_argument("--output", type=Path, default=ROOT / "sensitivity_curve.png")
    parser.add_argument("--no-plot", action="store_true", help="run the sweep without Matplotlib")
    args = parser.parse_args()

    if not args.exe.is_file():
        parser.error(f"executable not found: {args.exe}")

    cache_sizes = [1024 * 2**step for step in range(11)]
    misses = []
    for size in cache_sizes:
        result = subprocess.run(
            [str(args.exe), "--cache-size", str(size), "--json"],
            check=True,
            capture_output=True,
            text=True,
        )
        data = json.loads(result.stdout)
        if data["l1_cache_size"] != size or data["instructions_retired"] <= 0:
            raise RuntimeError(f"invalid simulator result at cache size {size}")
        misses.append(data["cache_misses"])
        print(f"Size: {size // 1024:6d} KB -> Misses: {misses[-1]}")

    if args.no_plot:
        return

    import matplotlib.pyplot as plt

    plt.figure(figsize=(10, 6))
    plt.plot([size / 1024 for size in cache_sizes], misses, marker="o")
    plt.xscale("log")
    plt.xlabel("L1 Cache Size (KB)")
    plt.ylabel("Total Cache Misses")
    plt.title("MatMul cache-size sensitivity")
    plt.grid(True, which="both", alpha=0.5)
    plt.axvline(x=48, color="r", linestyle="--", label="Working set (48 KB)")
    plt.legend()
    plt.savefig(args.output)
    print(f"Saved plot to {args.output}")


if __name__ == "__main__":
    main()
