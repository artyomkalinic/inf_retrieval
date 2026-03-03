import asyncio
import hashlib
import json
import logging
import sys
import time
from urllib.parse import urlparse, urlunparse

import aiohttp
import yaml
from motor.motor_asyncio import AsyncIOMotorClient

LOG = logging.getLogger("crawler")


def load_config(path):
    with open(path, "r", encoding="utf-8") as fh:
        return yaml.safe_load(fh)


async def connect_db(cfg):
    client = AsyncIOMotorClient(cfg["db"]["host"], cfg["db"]["port"])
    db = client[cfg["db"]["name"]]
    return client, db


async def load_crawled_urls(db):
    crawled = {}
    async for doc in db.documents.find({}, {"url": 1, "content_hash": 1}):
        crawled[doc["url"]] = doc.get("content_hash", "")
    return crawled


def normalize_url(url):
    p = urlparse(url)
    return urlunparse((p.scheme.lower(), p.netloc.lower(), p.path.rstrip("/"), "", "", ""))


def compute_hash(text):
    return hashlib.md5(text.encode("utf-8", errors="replace")).hexdigest()


async def save_document(db, url, raw_html, source_name, content_hash, crawled):
    now = int(time.time())
    if url in crawled:
        if crawled[url] == content_hash:
            return False
        await db.documents.update_one(
            {"url": url},
            {"$set": {"raw_html": raw_html, "content_hash": content_hash, "crawl_time": now}},
        )
        crawled[url] = content_hash
        return True
    await db.documents.insert_one({
        "url": url, "raw_html": raw_html, "source": source_name,
        "crawl_time": now, "content_hash": content_hash,
    })
    crawled[url] = content_hash
    return True


EPMC_SEARCH = "https://www.ebi.ac.uk/europepmc/webservices/rest/search"


async def fetch_epmc_page(session, cursor, page_size=1000):
    params = {
        "query": "(cancer OR oncology OR tumor OR neoplasm) AND (SRC:PMC) AND (LANG:eng) AND (HAS_ABSTRACT:y)",
        "resultType": "core",
        "pageSize": str(page_size),
        "format": "json",
        "cursorMark": cursor,
    }
    url = EPMC_SEARCH + "?" + "&".join(f"{k}={v}" for k, v in params.items())
    try:
        async with session.get(url) as resp:
            if resp.status != 200:
                LOG.warning("EPMC HTTP %d", resp.status)
                return [], None
            data = await resp.json(content_type=None)
            results = data.get("resultList", {}).get("result", [])
            next_cur = data.get("nextCursorMark")
            return results, next_cur
    except Exception as exc:
        LOG.error("EPMC fetch error: %s", exc)
        return [], None


def epmc_result_to_html(r):
    title = r.get("title", "Untitled")
    abstract = r.get("abstractText", "")
    authors = ", ".join(
        f"{a.get('firstName','')} {a.get('lastName','')}"
        for a in (r.get("authorList", {}).get("author", []) or [])
    )
    journal = r.get("journalTitle", "")
    year = r.get("pubYear", "")
    doi = r.get("doi", "")
    kw_data = r.get("keywordList")
    keywords = ""
    if kw_data and isinstance(kw_data, dict):
        kw_list = kw_data.get("keyword", []) or []
        if kw_list:
            if isinstance(kw_list[0], str):
                keywords = "; ".join(kw_list)
            elif isinstance(kw_list[0], dict):
                keywords = "; ".join(kw.get("keyword", "") for kw in kw_list)

    return (
        f"<html><head><title>{title}</title></head><body>"
        f"<h1>{title}</h1>"
        f"<p class='authors'>{authors}</p>"
        f"<p class='journal'>{journal} ({year})</p>"
        f"<p class='doi'>{doi}</p>"
        f"<p class='keywords'>{keywords}</p>"
        f"<div class='abstract'>{abstract}</div>"
        f"</body></html>"
    )


async def crawl_epmc(session, db, config, crawled):
    target = 26000
    for s in config["sources"]:
        if s["name"] == "pubmed_central":
            target = s["target_count"]

    already = sum(1 for u in crawled if "europepmc.org" in u or "ncbi.nlm.nih.gov/pmc" in u)
    LOG.info("EPMC: %d already in DB, target %d", already, target)
    if already >= target:
        return

    cursor = "*"
    saved = 0
    batch_num = 0
    batch_to_insert = []
    now = int(time.time())

    while saved + already < target:
        batch_num += 1
        results, next_cursor = await fetch_epmc_page(session, cursor, page_size=1000)
        if not results:
            LOG.warning("EPMC: empty page at batch %d, stopping", batch_num)
            break

        for r in results:
            if saved + already >= target:
                break

            pmcid = r.get("pmcid")
            if not pmcid:
                continue

            article_url = normalize_url(f"https://europepmc.org/article/PMC/{pmcid}")
            if article_url in crawled:
                continue

            abstract = r.get("abstractText", "")
            if len(abstract) < 100:
                continue

            html_doc = epmc_result_to_html(r)
            content_hash = compute_hash(html_doc)

            batch_to_insert.append({
                "url": article_url,
                "raw_html": html_doc,
                "source": "pubmed_central",
                "crawl_time": now,
                "content_hash": content_hash,
            })
            crawled[article_url] = content_hash
            saved += 1

        if batch_to_insert:
            try:
                await db.documents.insert_many(batch_to_insert, ordered=False)
            except Exception as exc:
                LOG.warning("insert_many partial error: %s", exc)
            batch_to_insert = []

        LOG.info("EPMC batch %d: %d new (%d total)", batch_num, saved, saved + already)

        if not next_cursor or next_cursor == cursor:
            LOG.info("EPMC: no more pages")
            break
        cursor = next_cursor

    LOG.info("EPMC done — %d new documents saved", saved)


CT_API = "https://clinicaltrials.gov/api/v2/studies"


async def fetch_ct_page(session, page_token=None, page_size=100):
    params = {"query.cond": "cancer", "pageSize": str(page_size)}
    if page_token:
        params["pageToken"] = page_token
    url = CT_API + "?" + "&".join(f"{k}={v}" for k, v in params.items())
    try:
        async with session.get(url) as resp:
            if resp.status != 200:
                LOG.warning("CT HTTP %d", resp.status)
                return [], None
            data = await resp.json(content_type=None)
            return data.get("studies", []), data.get("nextPageToken")
    except Exception as exc:
        LOG.error("CT fetch error: %s", exc)
        return [], None


def ct_study_to_html(study):
    proto = study.get("protocolSection", {})
    ident = proto.get("identificationModule", {})
    desc = proto.get("descriptionModule", {})
    status_mod = proto.get("statusModule", {})
    design = proto.get("designModule", {})
    conds = proto.get("conditionsModule", {})
    arms = proto.get("armsInterventionsModule", {})

    nct_id = ident.get("nctId", "")
    title = ident.get("officialTitle", ident.get("briefTitle", "Untitled"))
    brief_summary = desc.get("briefSummary", "")
    detailed_desc = desc.get("detailedDescription", "")
    status = status_mod.get("overallStatus", "")
    phase_list = (design.get("phases") or [])
    phases = ", ".join(phase_list) if phase_list else ""
    conditions = ", ".join(conds.get("conditions", []))
    interventions = []
    for arm in (arms.get("interventions") or []):
        interventions.append(f"{arm.get('type','')}: {arm.get('name','')}")
    interv_text = "; ".join(interventions)

    html = (
        f"<html><head><title>{title}</title></head><body>"
        f"<h1>{title}</h1>"
        f"<p class='nctid'>{nct_id}</p>"
        f"<p class='status'>{status}</p>"
        f"<p class='phases'>{phases}</p>"
        f"<p class='conditions'>{conditions}</p>"
        f"<p class='interventions'>{interv_text}</p>"
        f"<div class='summary'>{brief_summary}</div>"
        f"<div class='description'>{detailed_desc}</div>"
        f"</body></html>"
    )
    return nct_id, html


async def crawl_clinicaltrials(session, db, config, crawled):
    target = 7000
    for s in config["sources"]:
        if s["name"] == "clinicaltrials":
            target = s["target_count"]

    already = sum(1 for u in crawled if "clinicaltrials.gov" in u)
    LOG.info("CT: %d already in DB, target %d", already, target)
    if already >= target:
        return

    saved = 0
    next_token = None
    batch_num = 0
    batch_to_insert = []
    now = int(time.time())

    while saved + already < target:
        batch_num += 1
        studies, next_token = await fetch_ct_page(session, next_token, page_size=100)
        if not studies:
            LOG.info("CT: no more studies")
            break

        for study in studies:
            if saved + already >= target:
                break

            nct_id, html_doc = ct_study_to_html(study)
            if not nct_id:
                continue

            study_url = normalize_url(f"https://clinicaltrials.gov/study/{nct_id}")
            if study_url in crawled:
                continue

            if len(html_doc) < 300:
                continue

            content_hash = compute_hash(html_doc)
            batch_to_insert.append({
                "url": study_url,
                "raw_html": html_doc,
                "source": "clinicaltrials",
                "crawl_time": now,
                "content_hash": content_hash,
            })
            crawled[study_url] = content_hash
            saved += 1

        if batch_to_insert:
            try:
                await db.documents.insert_many(batch_to_insert, ordered=False)
            except Exception as exc:
                LOG.warning("CT insert_many error: %s", exc)
            batch_to_insert = []

        if saved % 500 < 100:
            LOG.info("CT batch %d: %d new (%d total)", batch_num, saved, saved + already)

        if not next_token:
            break
        await asyncio.sleep(0.05)

    LOG.info("CT done — %d new documents saved", saved)


async def run(config_path):
    config = load_config(config_path)
    LOG.info("Loaded config: %s", config_path)

    client, db = await connect_db(config)
    await db.documents.create_index("url", unique=True)

    crawled = await load_crawled_urls(db)
    LOG.info("Resuming with %d previously crawled URLs", len(crawled))

    timeout = aiohttp.ClientTimeout(total=120, connect=30)
    headers = {"User-Agent": config["logic"]["user_agent"]}

    async with aiohttp.ClientSession(timeout=timeout, headers=headers) as session:
        await crawl_epmc(session, db, config, crawled)
        await crawl_clinicaltrials(session, db, config, crawled)

    LOG.info("Finished. Total documents: %d", len(crawled))
    client.close()


def main():
    if len(sys.argv) != 2:
        print("Usage: python crawler.py <config.yaml>")
        sys.exit(1)

    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(levelname)s] %(message)s",
    )
    asyncio.run(run(sys.argv[1]))


if __name__ == "__main__":
    main()
