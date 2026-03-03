import argparse
import json
import os
import sys


def read_corpus_file(filepath):
    raw_size = os.path.getsize(filepath)
    with open(filepath, "r", encoding="utf-8") as f:
        lines = f.readlines()

    if len(lines) < 3:
        return None

    text = "".join(lines[2:])
    return {
        "title": lines[0].strip(),
        "url": lines[1].strip(),
        "raw_size": raw_size,
        "text_size": len(text.encode("utf-8")),
        "text_length": len(text),
    }


def compute_stats(corpus_dir):
    if not os.path.isdir(corpus_dir):
        print(f"Error: directory '{corpus_dir}' not found", file=sys.stderr)
        sys.exit(1)

    files = sorted(
        f for f in os.listdir(corpus_dir)
        if os.path.isfile(os.path.join(corpus_dir, f))
    )

    docs = [read_corpus_file(os.path.join(corpus_dir, f)) for f in files]
    docs = [d for d in docs if d is not None]

    if not docs:
        print("Error: no valid documents found", file=sys.stderr)
        sys.exit(1)

    n = len(docs)
    raw_sizes = [d["raw_size"] for d in docs]
    text_sizes = [d["text_size"] for d in docs]
    text_lengths = [d["text_length"] for d in docs]
    total_raw = sum(raw_sizes)
    total_text = sum(text_sizes)

    return {
        "total_documents": n,
        "total_raw_size_bytes": total_raw,
        "total_text_size_bytes": total_text,
        "avg_raw_size_bytes": round(total_raw / n, 2),
        "avg_text_size_bytes": round(total_text / n, 2),
        "avg_text_length_chars": round(sum(text_lengths) / n, 2),
        "min_raw_size_bytes": min(raw_sizes),
        "max_raw_size_bytes": max(raw_sizes),
        "min_text_size_bytes": min(text_sizes),
        "max_text_size_bytes": max(text_sizes),
    }


def format_size(b):
    if b >= 1024 * 1024:
        return f"{b / (1024 * 1024):.2f} MB"
    if b >= 1024:
        return f"{b / 1024:.2f} KB"
    return f"{b} B"


def print_stats(stats):
    print("=" * 50)
    print("         CORPUS STATISTICS")
    print("=" * 50)
    print(f"  Total documents:     {stats['total_documents']}")
    print(f"  Total raw size:      {format_size(stats['total_raw_size_bytes'])}")
    print(f"  Total text size:     {format_size(stats['total_text_size_bytes'])}")
    print(f"  Avg document size:   {format_size(stats['avg_raw_size_bytes'])}")
    print(f"  Avg text size:       {format_size(stats['avg_text_size_bytes'])}")
    print(f"  Avg text length:     {stats['avg_text_length_chars']:.0f} chars")
    print(f"  Min document size:   {format_size(stats['min_raw_size_bytes'])}")
    print(f"  Max document size:   {format_size(stats['max_raw_size_bytes'])}")
    print(f"  Min text size:       {format_size(stats['min_text_size_bytes'])}")
    print(f"  Max text size:       {format_size(stats['max_text_size_bytes'])}")
    print("=" * 50)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--corpus-dir",
        default=os.path.join(os.path.dirname(__file__), "..", "corpus"),
    )
    args = parser.parse_args()

    stats = compute_stats(os.path.abspath(args.corpus_dir))
    print_stats(stats)

    output_path = os.path.join(os.path.dirname(__file__), "stats.json")
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(stats, f, indent=2, ensure_ascii=False)
    print(f"\nSaved to {output_path}")


if __name__ == "__main__":
    main()
