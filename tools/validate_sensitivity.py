import subprocess
import matplotlib.pyplot as plt
import os
import re

# CONFIG
SIM_EXE = "./cpu-sim"
OUTPUT_IMG = "sensitivity_curve.png"

# Sweep Cache Size: 1KB to 1MB (powers of 2)
cache_sizes = [1024 * (2**i) for i in range(0, 11)] 
misses = []
sizes_kb = []

print(f"Running Sensitivity Sweep using {SIM_EXE}...")

if not os.path.exists(SIM_EXE):
    print(f"Error: Executable {SIM_EXE} not found. Did you compile?")
    exit(1)

for size in cache_sizes:
    # Run simulator
    result = subprocess.run([SIM_EXE, str(size)], capture_output=True, text=True)
    
    # Parse the human-readable output for "Cache Misses: <number>"
    # We use Regex to be robust
    match = re.search(r"Cache Misses:\s+(\d+)", result.stdout)
    
    if match:
        miss_count = int(match.group(1))
        misses.append(miss_count)
        sizes_kb.append(size / 1024)
        print(f"Size: {size/1024:6.0f} KB -> Misses: {miss_count}")
    else:
        print(f"Failed to parse output for size {size}. Got:\n{result.stdout}")

# Plotting
plt.figure(figsize=(10, 6))
plt.plot(sizes_kb, misses, marker='o', linestyle='-', color='b', linewidth=2)
plt.xscale('log')
plt.xlabel('L1 Cache Size (KB)')
plt.ylabel('Total Cache Misses')
plt.title('Validation: MatMul Working Set vs Cache Capacity')
plt.grid(True, which="both", ls="-", alpha=0.5)
plt.axvline(x=48, color='r', linestyle='--', label='Working Set (~48KB)')
plt.legend()
plt.savefig(OUTPUT_IMG)
print(f"\nValidation Graph saved to {OUTPUT_IMG}")