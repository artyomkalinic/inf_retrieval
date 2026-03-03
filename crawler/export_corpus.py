import argparse
import logging
import os
import sys

import yaml
from lxml import html as lxml_html
from pymongo import MongoClient

LOG = logging.getLogger("export_corpus")


def load_config(path):
    with open(path, "r", encoding="utf-8") as fh:
        return yaml.safe_load(fh)


def extract_text_from_html(raw_html):
    title = ""
    body_text = ""

    try:
        tree = lxml_html.fromstring(raw_html)
    except Exception as exc:
        LOG.warning("lxml parse error: %s", exc)
        return title, raw_html

    title_el = tree.find(".//title")
    if title_el is not None and title_el.text:
        title = title_el.text.strip()

    body_el = tree.find(".//article")
    if body_el is None:
        body_el = tree.find(".//div[@id='main-content']")
    if body_el is None:
        body_el = tree.find(".//div[@role='main']")
    if body_el is None:
        body_el = tree.body if tree.body is not None else tree

    raw_text = body_el.text_content()
    cleaned_lines = [" ".join(line.split()) for line in raw_text.split("\n") if line.strip()]
    body_text = "\n".join(cleaned_lines)

    return title, body_text


def export_documents(config, output_dir):
    client = MongoClient(config["db"]["host"], config["db"]["port"])
    db = client[config["db"]["name"]]

    os.makedirs(output_dir, exist_ok=True)

    total = db.documents.count_documents({})
    LOG.info("Documents in MongoDB: %d", total)

    manifest_path = os.path.join(output_dir, "manifest.tsv")
    exported = 0
    errors = 0
    numeric_id = 0

    with open(manifest_path, "w", encoding="utf-8") as manifest:
        manifest.write("doc_id\turl\ttitle\ttext_size_bytes\n")

        for doc in db.documents.find():
            doc_id = str(numeric_id)
            url = doc.get("url", "")
            raw_html = doc.get("raw_html", "")

            try:
                title, body_text = extract_text_from_html(raw_html)
            except Exception as exc:
                LOG.error("Text extraction failed for %s: %s", url, exc)
                errors += 1
                continue

            if not body_text.strip():
                LOG.warning("Empty body for %s, skipping", url)
                errors += 1
                continue

            doc_path = os.path.join(output_dir, f"{doc_id}.txt")
            with open(doc_path, "w", encoding="utf-8") as fh:
                fh.write(title + "\n")
                fh.write(url + "\n")
                fh.write(body_text + "\n")

            text_size = len(body_text.encode("utf-8"))
            safe_title = title.replace("\t", " ").replace("\n", " ")
            manifest.write(f"{doc_id}\t{url}\t{safe_title}\t{text_size}\n")

            exported += 1
            numeric_id += 1
            if exported % 5000 == 0:
                LOG.info("Exported %d / %d", exported, total)

    client.close()
    LOG.info("Done: %d exported, %d errors", exported, errors)
    LOG.info("Manifest → %s", manifest_path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("config", help="Path to YAML config file")
    parser.add_argument("--output-dir", default="corpus")
    args = parser.parse_args()

    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(levelname)s] %(message)s",
    )

    config = load_config(args.config)
    export_documents(config, args.output_dir)


if __name__ == "__main__":
    main()
