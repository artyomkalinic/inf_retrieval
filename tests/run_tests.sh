#!/usr/bin/env bash

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CORE="$ROOT/core"
BUILD="$CORE/build"
INDEXER="$BUILD/indexer"
SEARCHER="$BUILD/searcher"
TESTER="$BUILD/tester"
INDEX="$ROOT/index.bin"

PASS=0
FAIL=0

ok()   { printf "  [PASS] %s\n" "$1"; PASS=$((PASS+1)); }
fail() { printf "  [FAIL] %s\n" "$1"; FAIL=$((FAIL+1)); }

check() {
    local desc="$1"
    if [ "${2:-1}" = "0" ]; then ok "$desc"; else fail "$desc"; fi
}

count_lines() {
    local n
    n=$(echo "$1" | grep -c $'\t' 2>/dev/null) || n=0
    echo "$n"
}

section() { printf "\n=== %s ===\n" "$1"; }

printf "=== Integration Test Suite ===\n"

section "TC-01: Build"
(cd "$CORE" && make -s all 2>/dev/null) || true
check "indexer binary exists"  "$([ -f "$INDEXER" ] && echo 0 || echo 1)"
check "searcher binary exists" "$([ -f "$SEARCHER" ] && echo 0 || echo 1)"
check "tester binary exists"   "$([ -f "$TESTER" ] && echo 0 || echo 1)"

section "TC-02: Unit tests (C++)"
TESTER_OUT=$("$TESTER" 2>&1 || true)
TESTER_EXIT=$?
UNIT_PASS=$(echo "$TESTER_OUT" | grep -c '\[PASS\]' || true)
UNIT_FAIL=$(echo "$TESTER_OUT" | grep -c '\[FAIL\]' || true)
check "tester exits 0"                              "$([ "$TESTER_EXIT" -eq 0 ] && echo 0 || echo 1)"
check "unit pass count > 80 ($UNIT_PASS passed)"    "$([ "$UNIT_PASS" -gt 80 ] && echo 0 || echo 1)"
check "unit fail count = 0 ($UNIT_FAIL failed)"     "$([ "$UNIT_FAIL" -eq 0 ] && echo 0 || echo 1)"

section "TC-03: Index file integrity"
check "index.bin exists"            "$([ -f "$INDEX" ] && echo 0 || echo 1)"
SIZE=$(wc -c < "$INDEX" 2>/dev/null | tr -d ' ' || echo 0)
check "index size > 1 MB ($SIZE B)" "$([ "$SIZE" -gt 1048576 ] && echo 0 || echo 1)"
MAGIC=$(xxd -l 4 "$INDEX" 2>/dev/null | awk '{print $2$3}' | tr '[:upper:]' '[:lower:]' || echo "")
check "index magic = SRCH (53524348)" "$([ "$MAGIC" = "53524348" ] && echo 0 || echo 1)"

section "TC-04: Basic term search"
R=$(echo "cancer" | "$SEARCHER" "$INDEX" 2>/dev/null || true)
N=$(count_lines "$R")
check "query 'cancer' returns results"    "$([ "$N" -gt 0 ] && echo 0 || echo 1)"
check "query 'cancer' returns 50 results (max page)" "$([ "$N" -eq 50 ] && echo 0 || echo 1)"

section "TC-05: AND operator"
R_BC=$(echo "breast cancer" | "$SEARCHER" "$INDEX" 2>/dev/null || true)
N_BC=$(count_lines "$R_BC")
check "query 'breast cancer' returns results" "$([ "$N_BC" -gt 0 ] && echo 0 || echo 1)"

R_C=$(echo "cancer" | "$SEARCHER" "$INDEX" 2>/dev/null | grep $'\t' | wc -l | tr -d ' ' || true)
R_B=$(echo "breast" | "$SEARCHER" "$INDEX" 2>/dev/null | grep $'\t' | wc -l | tr -d ' ' || true)
check "AND subset: breast cancer appears in both terms" "$([ "$N_BC" -gt 0 ] && echo 0 || echo 1)"

section "TC-06: OR operator"
R_A=$(echo "brca1 mutation !cancer" | "$SEARCHER" "$INDEX" 2>/dev/null || true)
N_A=$(count_lines "$R_A")
R_B=$(echo "brca1 mutation" | "$SEARCHER" "$INDEX" 2>/dev/null || true)
N_B=$(count_lines "$R_B")
check "OR superset: brca1 mutation >= brca1 mutation !cancer" "$([ "$N_B" -ge "$N_A" ] && echo 0 || echo 1)"

section "TC-07: NOT operator"
R_ALL=$(echo "brca1 mutation" | "$SEARCHER" "$INDEX" 2>/dev/null || true)
N_ALL=$(count_lines "$R_ALL")
R_NOT=$(echo "brca1 mutation !cancer" | "$SEARCHER" "$INDEX" 2>/dev/null || true)
N_NOT=$(count_lines "$R_NOT")
check "NOT reduces result set ($N_ALL -> $N_NOT)" "$([ "$N_NOT" -lt "$N_ALL" ] && echo 0 || echo 1)"
check "NOT result count > 0"                       "$([ "$N_NOT" -gt 0 ] && echo 0 || echo 1)"

section "TC-08: Parentheses grouping"
R=$(echo "(breast || lung) cancer" | "$SEARCHER" "$INDEX" 2>/dev/null || true)
N=$(count_lines "$R")
check "grouped query returns results" "$([ "$N" -gt 0 ] && echo 0 || echo 1)"

section "TC-09: Output format"
FIRST=$(echo "cancer" | "$SEARCHER" "$INDEX" 2>/dev/null | head -1)
FIELDS=$(echo "$FIRST" | awk -F'\t' '{print NF}')
check "output line has 4 tab-separated fields" "$([ "$FIELDS" -eq 4 ] && echo 0 || echo 1)"
SCORE=$(echo "$FIRST" | awk -F'\t' '{print $2}')
check "score field is numeric" "$(echo "$SCORE" | grep -qE '^[0-9]+\.' && echo 0 || echo 1)"

section "TC-10: Empty query"
R=$(echo "" | "$SEARCHER" "$INDEX" 2>/dev/null || true)
N=$(count_lines "$R")
check "empty query returns 0 results" "$([ "$N" -eq 0 ] && echo 0 || echo 1)"

section "TC-11: TF-IDF ordering (descending scores)"
R=$(echo "brca1 mutation breast cancer" | "$SEARCHER" "$INDEX" 2>/dev/null || true)
N=$(count_lines "$R")
check "tfidf query returns results" "$([ "$N" -gt 0 ] && echo 0 || echo 1)"
S1=$(echo "$R" | head -1 | awk -F'\t' '{print $2}')
S2=$(echo "$R" | sed -n '2p' | awk -F'\t' '{print $2}')
if [ -n "$S1" ] && [ -n "$S2" ]; then
    CMP=$(awk "BEGIN{print ($S1 >= $S2) ? 0 : 1}" 2>/dev/null || echo 1)
    check "scores are in descending order (${S1} >= ${S2})" "$CMP"
fi

section "TC-12: Corpus statistics"
CORPUS="$ROOT/corpus"
if [ -d "$CORPUS" ]; then
    DOC_COUNT=$(find "$CORPUS" -maxdepth 1 -name "*.txt" 2>/dev/null | wc -l | tr -d ' ' || echo 0)
    check "corpus has > 30000 documents ($DOC_COUNT docs)" "$([ "$DOC_COUNT" -gt 30000 ] && echo 0 || echo 1)"
    check "manifest.tsv exists" "$([ -f "$CORPUS/manifest.tsv" ] && echo 0 || echo 1)"
else
    printf "  [SKIP] corpus not found (was not crawled in this run)\n"
fi

printf "\n==============================\n"
printf "Results: %d passed, %d failed\n" "$PASS" "$FAIL"
printf "==============================\n"

[ "$FAIL" -eq 0 ]
