import sys
import soundfile as sf
import matplotlib.pyplot as plt

files = sys.argv[1:]
if not files:
    print("usage: plot.py <file.wav> [file.wav ...]")
    sys.exit(1)

fig, axes = plt.subplots(len(files), 1, sharex=True, figsize=(14, 4 * len(files)))
if len(files) == 1:
    axes = [axes]

for ax, path in zip(axes, files):
    data, sr = sf.read(path)
    time = [i / sr for i in range(len(data))]
    ax.plot(time, data, linewidth=0.3)
    ax.set_title(path)
    ax.set_ylabel("amplitude")

axes[-1].set_xlabel("time (s)")
plt.tight_layout()
plt.show()
