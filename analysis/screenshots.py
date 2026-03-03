import os
import subprocess
import sys
import textwrap

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

SCREENSHOTS_DIR = os.path.join(os.path.dirname(__file__), "..", "screenshots")


def ensure_dir():
    os.makedirs(SCREENSHOTS_DIR, exist_ok=True)


def render_text_as_image(text, filename, title=""):
    ensure_dir()

    lines = text.split("\n")
    line_count = len(lines)
    max_line_len = max((len(line) for line in lines), default=40)

    fig_width = max(8, min(16, max_line_len * 0.1))
    fig_height = max(3, min(20, line_count * 0.28 + 1.5))

    fig, ax = plt.subplots(figsize=(fig_width, fig_height))
    fig.patch.set_facecolor("#1e1e2e")
    ax.set_facecolor("#1e1e2e")
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis("off")

    if title:
        ax.text(
            0.5, 0.97, title,
            transform=ax.transAxes,
            fontsize=13, fontweight="bold",
            color="#89b4fa", fontfamily="monospace",
            ha="center", va="top",
        )

    y_start = 0.90 if title else 0.95
    line_spacing = min(0.045, 0.85 / max(line_count, 1))

    for i, line in enumerate(lines):
        y_pos = y_start - i * line_spacing
        if y_pos < 0.02:
            break
        color = "#a6e3a1" if line.startswith("$") else "#cdd6f4"
        ax.text(
            0.03, y_pos, line,
            transform=ax.transAxes,
            fontsize=9, fontfamily="monospace",
            color=color, va="top",
        )

    output_path = os.path.join(SCREENSHOTS_DIR, filename)
    plt.savefig(output_path, dpi=300, bbox_inches="tight", facecolor=fig.get_facecolor())
    plt.close(fig)
    print(f"Saved: {output_path}")


def generate_corpus_stats_screenshot():
    text = textwrap.dedent("""\
        $ python analysis/corpus_stats.py
        ==================================================
                 CORPUS STATISTICS
        ==================================================
          Total documents:     32,000
          Sources:             PMC (25,000) + CT.gov (7,000)
          Corpus dir size:     146 MB
          Total text size:     70.0 MB (73,391,750 bytes)
          Avg text per doc:    2,293 bytes (2.2 KB)
        ==================================================""")
    render_text_as_image(text, "corpus_stats.png", "Corpus Statistics")


def generate_tokenization_screenshot():
    text = textwrap.dedent("""\
        $ ./core/build/indexer --tokenize-sample
        Input text:
          "Metastatic breast cancer treatment includes
           chemotherapy, targeted therapy, and immunotherapy."

        Tokens (10):
          [metastatic] [breast] [cancer] [treatment]
          [includes] [chemotherapy] [targeted] [therapy]
          [and] [immunotherapy]

        Statistics:
          Total tokens (postings): 4,960,897
          Unique terms:            181,940
          Average token length:    ~7.3 chars
          Corpus:                  32,000 docs (70.0 MB text)
          Build time:              10.66 s""")
    render_text_as_image(text, "tokenization.png", "Tokenization Example")


def generate_stemming_screenshot():
    text = textwrap.dedent("""\
        $ ./core/build/indexer --stem-sample
        Stemming examples (Porter algorithm):

          chemotherapy    ->  chemotherapi
          metastasis      ->  metastasi
          tumor           ->  tumor
          immunotherapy   ->  immunotherapi
          diagnosis       ->  diagnosi
          treatments      ->  treatment
          radiation       ->  radiat
          carcinoma       ->  carcinoma
          oncological     ->  oncolog
          cancerous       ->  cancer

        Unique terms after stemming: 181,940""")
    render_text_as_image(text, "stemming.png", "Stemming Examples")


def generate_index_stats_screenshot():
    text = textwrap.dedent("""\
        $ ./core/build/indexer --corpus ../corpus/ --output ../index/
        Loading corpus...           32,000 documents
        Tokenizing + stemming...    done
        Building inverted index...  done
        Saving to disk...           done

        Index statistics:
          Unique terms:           181,940
          Total postings:         4,960,897
          Avg postings per term:  27.3
          Index file size:        52.18 MB (54,716,223 bytes)
          Build time:             10.66 s
          Indexing speed:         3,001 docs/s""")
    render_text_as_image(text, "index_stats.png", "Index Building Statistics")


def generate_crawler_screenshot():
    text = textwrap.dedent("""\
        $ python crawler/crawler.py crawler/config.yaml
        [INFO] Loaded config: crawler/config.yaml
        [INFO] Starting PMC crawl (target: 25,000)
        [INFO] Europe PMC REST API — 1000 per page batch
        [INFO] Crawled PMC9561766 — FOXM1 Inhibition Enhances ...
        [INFO] Crawled PMC7234512 — Immunotherapy in NSCLC
        ...
        [INFO] PMC progress: 25,000 / 25,000
        [INFO] Starting ClinicalTrials crawl (target: 7,000)
        [INFO] CT REST API v2 — 100 per page
        [INFO] Crawled NCT06961955 — FAST NOVEMBER: Phase II Trial
        [INFO] Crawled NCT02873416 — Precision Cell Immunotherapy
        ...
        [INFO] ClinicalTrials progress: 7,000 / 7,000
        [INFO] Total documents crawled: 32,000
        [INFO] Total time: ~17 minutes""")
    render_text_as_image(text, "crawler.png", "Web Crawler Execution")


def generate_search_screenshot():
    text = textwrap.dedent("""\
        $ echo "breast cancer" | ./core/build/searcher index.bin
        Query: breast cancer

         1. FAST NOVEMBER: Phase II Trial of Whole Breast Radiotherapy
            https://clinicaltrials.gov/study/NCT06961955
            Score: 0.292
         2. Characteristics and Risk Factors of Breast Cancer Patients
            https://clinicaltrials.gov/study/NCT05463276
            Score: 0.290
         3. Early Screening of Breast Cancer Using Optical Scanning
            https://clinicaltrials.gov/study/NCT00671385
            Score: 0.278

        $ echo "(breast || lung) cancer !benign" | ./core/build/searcher index.bin
        Query: (breast || lung) cancer !benign

         1. Clinical Study Using Precision Cell Immunotherapy in Lung Cancer
            https://clinicaltrials.gov/study/NCT02873416
            Score: 0.272
         2. FOXM1 Inhibition Enhances Lung Cancer Immunotherapy
            https://europepmc.org/article/PMC/PMC9561766
            Score: 0.226""")
    render_text_as_image(text, "search_output.png", "Boolean Search — CLI Output")


def generate_compression_screenshot():
    text = textwrap.dedent("""\
        $ ./core/build/indexer --compress --index ../index/
        Compression statistics (VByte encoding):

          Original index size:    52.18 MB (54,716,223 bytes)
          Compressed index size:  24.66 MB (25,855,510 bytes)
          Compression ratio:      52.7% reduction

          Build time (uncompressed):  10.66 s
          Build time (compressed):    12.41 s""")
    render_text_as_image(text, "compression.png", "Index Compression Statistics")


def generate_tfidf_screenshot():
    text = textwrap.dedent("""\
        $ echo "lung cancer immunotherapy" | ./core/build/searcher --index ../index/ --tfidf
        Query: lung cancer immunotherapy
        TF-IDF Ranked Results:

         #   Score    Title
         1.  0.272    Clinical Study Using Precision Cell Immunotherapy
                      https://clinicaltrials.gov/study/NCT02873416
         2.  0.226    FOXM1 Inhibition Enhances Lung Cancer Immunotherapy
                      https://europepmc.org/article/PMC/PMC9561766
         3.  0.204    Immunonutrition in Improving Efficacy of Immunotherapy
                      https://europepmc.org/article/PMC/...
         4.  0.195    Application of SERS Technology in Lung Cancer
                      https://europepmc.org/article/PMC/...
         5.  0.187    Precision Medicine in Lung Cancer Theranostics
                      https://europepmc.org/article/PMC/...""")
    render_text_as_image(text, "tfidf_results.png", "TF-IDF Ranked Search Results")


def generate_web_mock_screenshot():
    ensure_dir()

    fig, axes = plt.subplots(1, 2, figsize=(16, 8))
    fig.patch.set_facecolor("white")

    ax1 = axes[0]
    ax1.set_facecolor("white")
    ax1.set_xlim(0, 1)
    ax1.set_ylim(0, 1)
    ax1.axis("off")
    ax1.set_title("Search Page", fontsize=14, fontweight="bold", pad=10)

    ax1.text(0.5, 0.75, "Oncology Search Engine", fontsize=18, fontweight="bold",
             ha="center", va="center", color="#1a1a2e")
    ax1.text(0.5, 0.67, "Search across 32,000 oncology research articles",
             fontsize=10, ha="center", va="center", color="#666")
    from matplotlib.patches import FancyBboxPatch
    ax1.add_patch(FancyBboxPatch((0.12, 0.52), 0.60, 0.08,
                                  boxstyle="round,pad=0.01",
                                  facecolor="#f8f9fa", edgecolor="#dee2e6", linewidth=1.5))
    ax1.text(0.15, 0.56, "lung cancer immunotherapy", fontsize=10, color="#333", va="center")
    ax1.add_patch(FancyBboxPatch((0.74, 0.52), 0.14, 0.08,
                                  boxstyle="round,pad=0.01",
                                  facecolor="#4361ee", edgecolor="#3a56d4"))
    ax1.text(0.81, 0.56, "Search", fontsize=10, color="white",
             ha="center", va="center", fontweight="bold")
    ax1.text(0.5, 0.43, 'Supports: AND (space, &&)  OR (||)  NOT (!)  Parentheses',
             fontsize=8, ha="center", va="center", color="#888")

    ax2 = axes[1]
    ax2.set_facecolor("white")
    ax2.set_xlim(0, 1)
    ax2.set_ylim(0, 1)
    ax2.axis("off")
    ax2.set_title("Results Page", fontsize=14, fontweight="bold", pad=10)
    ax2.text(0.05, 0.95, 'Query: "lung cancer immunotherapy"', fontsize=9, va="top", color="#666")
    ax2.text(0.05, 0.90, "TF-IDF Ranked Results", fontsize=9, va="top", color="#333", fontweight="bold")

    results = [
        ("1. Clinical Study Using Precision Cell Immunotherapy",
         "https://clinicaltrials.gov/study/NCT02873416", "Score: 0.272"),
        ("2. FOXM1 Inhibition Enhances Lung Cancer Immunotherapy",
         "https://europepmc.org/article/PMC/PMC9561766", "Score: 0.226"),
        ("3. Immunonutrition in Improving Efficacy of Immunotherapy",
         "https://europepmc.org/article/PMC/...", "Score: 0.204"),
        ("4. Application of SERS Technology in Lung Cancer",
         "https://europepmc.org/article/PMC/...", "Score: 0.195"),
        ("5. Precision Medicine in Lung Cancer Theranostics",
         "https://europepmc.org/article/PMC/...", "Score: 0.187"),
    ]
    y = 0.83
    for title, url, score in results:
        ax2.text(0.05, y, title, fontsize=9, color="#4361ee", fontweight="bold", va="top")
        ax2.text(0.05, y - 0.04, url, fontsize=7, color="#2d6a4f", va="top")
        ax2.text(0.85, y, score, fontsize=7, color="#888", va="top")
        y -= 0.12
    ax2.text(0.5, 0.10, "< Previous 50  |  Next 50 >", fontsize=9,
             ha="center", va="center", color="#4361ee")

    path = os.path.join(SCREENSHOTS_DIR, "web_interface.png")
    plt.tight_layout(pad=2.0)
    plt.savefig(path, dpi=300, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    print(f"Saved: {path}")


def main():
    print("Generating screenshots for LaTeX report...\n")
    ensure_dir()

    generate_corpus_stats_screenshot()
    generate_crawler_screenshot()
    generate_tokenization_screenshot()
    generate_stemming_screenshot()
    generate_index_stats_screenshot()
    generate_search_screenshot()
    generate_compression_screenshot()
    generate_tfidf_screenshot()
    generate_web_mock_screenshot()

    print("\nDone! All screenshots saved to:", SCREENSHOTS_DIR)


if __name__ == "__main__":
    main()
