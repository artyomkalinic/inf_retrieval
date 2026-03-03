import argparse
import os

import matplotlib.pyplot as plt
import numpy as np


def read_frequencies(input_path):
    terms = []
    freqs = []
    with open(input_path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            parts = line.split("\t")
            if len(parts) != 2:
                continue
            try:
                freq = int(parts[1])
            except ValueError:
                continue
            terms.append(parts[0])
            freqs.append(freq)
    return terms, freqs


def plot_zipf(freqs, output_path):
    ranks = np.arange(1, len(freqs) + 1)
    freqs_array = np.array(freqs, dtype=float)

    c_value = freqs_array[0]
    zipf_theoretical = c_value / ranks

    plt.figure(figsize=(10, 7))
    plt.loglog(ranks, freqs_array, "b-", linewidth=1.5, label="Observed frequencies")
    plt.loglog(
        ranks, zipf_theoretical, "r--", linewidth=1.5,
        label=f"Zipf's law: f(r) = {c_value:.0f} / r",
    )
    plt.xlabel("Rank (log scale)", fontsize=13)
    plt.ylabel("Frequency (log scale)", fontsize=13)
    plt.title("Zipf's Law: Term Frequency Distribution", fontsize=15)
    plt.legend(fontsize=12)
    plt.grid(True, which="both", linestyle="--", alpha=0.4)
    plt.tight_layout()

    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
    plt.savefig(output_path, dpi=300, bbox_inches="tight")
    plt.close()
    print(f"Zipf plot saved to {output_path}")


def generate_sample_frequencies(n=50000):
    rng = np.random.default_rng(42)
    ranks = np.arange(1, n + 1)
    freqs = (320000 / np.power(ranks, 1.07)).astype(int)
    noise = rng.normal(1.0, 0.05, size=n)
    freqs = np.maximum((freqs * noise).astype(int), 1)
    return np.sort(freqs)[::-1].tolist()


def main():
    default_output = os.path.join(
        os.path.dirname(__file__), "..", "screenshots", "zipf_plot.png"
    )
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", help="Path to TSV file (term<TAB>frequency)")
    parser.add_argument("--output", default=default_output)
    parser.add_argument("--sample", action="store_true")
    args = parser.parse_args()

    if args.sample:
        print("Generating sample Zipf distribution (50,000 terms)...")
        freqs = generate_sample_frequencies()
    elif args.input:
        terms, freqs = read_frequencies(args.input)
        if not freqs:
            print("Error: no frequency data found in input file")
            return
        print(f"Loaded {len(freqs)} terms")
        print(f"Top-5 terms: {terms[:5]}")
    else:
        print("Error: provide --input or --sample")
        return

    print(f"Most frequent: {freqs[0]}, Least frequent: {freqs[-1]}")
    plot_zipf(freqs, args.output)


if __name__ == "__main__":
    main()
