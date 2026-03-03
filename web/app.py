import math
import os
import subprocess

from fastapi import FastAPI, Request, Query
from fastapi.responses import HTMLResponse
from fastapi.templating import Jinja2Templates

app = FastAPI(title="Oncology Search Engine")

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
templates = Jinja2Templates(directory=os.path.join(BASE_DIR, "templates"))

SEARCHER_PATH = os.path.join(BASE_DIR, "..", "core", "build", "searcher")
INDEX_PATH = os.path.join(BASE_DIR, "..", "index.bin")
RESULTS_PER_PAGE = 50


def run_search(query_text):
    if not os.path.isfile(SEARCHER_PATH):
        return [], "Searcher binary not found"

    try:
        result = subprocess.run(
            [SEARCHER_PATH, INDEX_PATH],
            input=query_text + "\n",
            capture_output=True,
            text=True,
            timeout=30,
        )
    except subprocess.TimeoutExpired:
        return [], "Search timed out"
    except FileNotFoundError:
        return [], "Searcher binary not found"

    if result.returncode != 0:
        return [], result.stderr.strip() or "Search failed"

    results = []
    for line in result.stdout.strip().split("\n"):
        if not line.strip():
            continue
        parts = line.split("\t")
        if len(parts) < 4:
            continue
        results.append({
            "doc_id": parts[0],
            "score": float(parts[1]),
            "title": parts[2],
            "url": parts[3],
        })

    return results, None


@app.get("/", response_class=HTMLResponse)
def index_page(request: Request):
    return templates.TemplateResponse("index.html", {"request": request})


@app.get("/search", response_class=HTMLResponse)
def search_page(
    request: Request,
    q: str = Query(""),
    page: int = Query(1, ge=1),
):
    if not q.strip():
        return templates.TemplateResponse("index.html", {"request": request})

    all_results, error = run_search(q)

    total_results = len(all_results)
    total_pages = max(1, math.ceil(total_results / RESULTS_PER_PAGE))
    page = min(page, total_pages)

    start_idx = (page - 1) * RESULTS_PER_PAGE
    end_idx = start_idx + RESULTS_PER_PAGE

    return templates.TemplateResponse("results.html", {
        "request": request,
        "query": q,
        "results": all_results[start_idx:end_idx],
        "total_results": total_results,
        "page": page,
        "total_pages": total_pages,
        "start_idx": start_idx,
        "error": error,
    })


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="127.0.0.1", port=8080)
