# This script was make using Gemini Flash 3.8
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import pandas as pd

configs = ["SameSocket", "DiffSocket", "DiffNode"]
data = {
    "Latency": ("latency.csv", "Latency (µs)"),
    "Bandwidth": ("bandwidth.csv", "Bandwidth (MB/s)"),
}
scales = [
    ("Linear", "linear", "linear"),
    ("Semi-Log", "log", "linear"),
    ("Log-Log", "log", "log"),
]

fig, axes = plt.subplots(2, 3, figsize=(16, 9))

for row, (metric, (csv_file, ylabel)) in enumerate(data.items()):
    df = pd.read_csv(csv_file)

    for col, (name, xscale, yscale) in enumerate(scales):
        ax = axes[row, col]
        sub_df = df[df["Size"] > 0] if xscale == "log" else df

        for cfg in configs:
            cols = [c for c in sub_df.columns if c.startswith(cfg)]
            mean = sub_df[cols].mean(axis=1)
            std = sub_df[cols].std(axis=1).fillna(0)

            # Mean curve
            line, = ax.plot(sub_df["Size"], mean, marker=".", label=cfg)

            # Variance band (clipped to avoid non-positive values on log-y)
            lower = (mean - std).clip(lower=sub_df[cols].min(axis=1))
            upper = mean + std
            ax.fill_between(sub_df["Size"], lower, upper, color=line.get_color(), alpha=0.2)

        ax.set_title(f"{metric} – {name}")
        ax.set_xlabel("Size (Bytes)")
        ax.set_ylabel(ylabel)
        # ax.set_yscale(yscale)

        if yscale == "log":
            ax.set_yscale("log", base=2)
            ax.yaxis.set_major_locator(ticker.LogLocator(base=2.0, numticks=12))
            ax.yaxis.set_major_formatter(ticker.LogFormatterMathtext(base=2))
        else:
            ax.set_yscale("linear")


        # Base-2 log scale for x-axis
        if xscale == "log":
            ax.set_xscale("log", base=2)
            ax.xaxis.set_major_locator(ticker.LogLocator(base=2.0, numticks=12))
            ax.xaxis.set_major_formatter(ticker.LogFormatterMathtext(base=2))
        else:
            ax.set_xscale("linear")

        ax.grid(True, which="both", ls="--", alpha=0.5)
        ax.legend()

plt.tight_layout()
plt.savefig("benchmark_plots.png", dpi=300)
plt.show()