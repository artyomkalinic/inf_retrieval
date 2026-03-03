#include "array.h"
#include "hashmap.h"
#include "str_utils.h"
#include "tokenizer.h"
#include "stemmer.h"
#include "compress.h"
#include "tfidf.h"
#include "query_parser.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

static int passed = 0;
static int failed = 0;

static void check(bool condition, const char* name) {
    if (condition) {
        printf("  [PASS] %s\n", name);
        passed++;
    } else {
        printf("  [FAIL] %s\n", name);
        failed++;
    }
}

static void section(const char* name) {
    printf("\n=== %s ===\n", name);
}

static void test_dynarray() {
    section("DynArray");

    DynArray arr(sizeof(int));
    check(arr.getCount() == 0, "initial count is 0");

    int v = 42;
    arr.push(&v);
    check(arr.getCount() == 1, "count after push = 1");
    check(*(int*)arr.at(0) == 42, "first element = 42");

    for (int i = 1; i < 20; i++) {
        arr.push(&i);
    }
    check(arr.getCount() == 20, "count after 20 pushes = 20");
    check(*(int*)arr.at(19) == 19, "last element = 19");

    arr.clear();
    check(arr.getCount() == 0, "count after clear = 0");

    int vals[] = {5, 3, 8, 1, 9, 2};
    DynArray arr2(sizeof(int));
    for (int i = 0; i < 6; i++) arr2.push(&vals[i]);

    arr2.sort([](const void* a, const void* b) -> int {
        return *(int*)a - *(int*)b;
    });
    check(*(int*)arr2.at(0) == 1, "sort: first = 1");
    check(*(int*)arr2.at(5) == 9, "sort: last = 9");
    check(*(int*)arr2.at(2) == 3, "sort: middle = 3");
}

static void test_hashmap() {
    section("HashMap");

    HashMap hm;
    check(hm.getCount() == 0, "initial count = 0");

    hm.put("cancer", 1);
    hm.put("tumor", 2);
    hm.put("oncology", 3);
    check(hm.getCount() == 3, "count after 3 inserts = 3");

    unsigned int val;
    check(hm.get("cancer", &val) && val == 1, "get cancer = 1");
    check(hm.get("tumor", &val) && val == 2, "get tumor = 2");
    check(hm.get("oncology", &val) && val == 3, "get oncology = 3");
    check(!hm.get("missing", &val), "missing key returns false");
    check(hm.contains("cancer"), "contains cancer");
    check(!hm.contains("missing"), "not contains missing");

    hm.put("cancer", 99);
    check(hm.get("cancer", &val) && val == 99, "update cancer = 99");

    hm.remove("tumor");
    check(!hm.contains("tumor"), "after remove: not contains tumor");
    check(hm.getCount() == 2, "count after remove = 2");

    HashMap hm2(4);
    for (int i = 0; i < 100; i++) {
        char key[32];
        snprintf(key, sizeof(key), "term%d", i);
        hm2.put(key, (unsigned int)i);
    }
    check(hm2.getCount() == 100, "hashmap grows: count = 100");
    check(hm2.get("term50", &val) && val == 50, "hashmap grows: get term50 = 50");
    check(hm2.get("term99", &val) && val == 99, "hashmap grows: get term99 = 99");
}

static void test_str_utils() {
    section("StrUtils");

    check(str_len("hello") == 5, "str_len hello = 5");
    check(str_len("") == 0, "str_len empty = 0");
    check(str_eq("abc", "abc"), "str_eq same");
    check(!str_eq("abc", "abd"), "str_eq different");
    check(str_cmp("abc", "abd") < 0, "str_cmp abc < abd");
    check(str_cmp("abd", "abc") > 0, "str_cmp abd > abc");

    char* dup = str_dup("hello");
    check(str_eq(dup, "hello"), "str_dup works");
    free(dup);

    char s[] = "Hello World";
    str_to_lower(s);
    check(str_eq(s, "hello world"), "str_to_lower works");

    check(str_to_int("42") == 42, "str_to_int 42");
    check(str_to_int("-7") == -7, "str_to_int -7");
    check(str_to_int("0") == 0, "str_to_int 0");
}

static void test_tokenizer() {
    section("Tokenizer");

    auto toks = tokenize("cancer treatment");
    check(toks.size() == 2, "tokenize: 2 tokens");
    check(toks[0].text == "cancer", "tokenize: first = cancer");
    check(toks[1].text == "treatment", "tokenize: second = treatment");

    auto toks2 = tokenize("BRCA1 mutation");
    check(toks2.size() == 2, "tokenize: uppercase lowered");
    check(toks2[0].text == "brca1", "tokenize: brca1");

    auto toks3 = tokenize("dose-dependent therapy");
    check(toks3.size() >= 3, "tokenize: hyphen splits");
    check(toks3[0].text == "dose", "tokenize hyphen: dose");
    check(toks3[1].text == "dependent", "tokenize hyphen: dependent");

    auto toks4 = tokenize("a it is");
    check(toks4.size() == 2, "tokenize: single-char token filtered");
    check(toks4[0].text == "it" && toks4[1].text == "is", "tokenize: 'it' and 'is' kept");

    auto toks5 = tokenize_simple("lung cancer treatment");
    check(toks5.size() == 3, "tokenize_simple: 3 tokens");
    check(toks5[0] == "lung", "tokenize_simple: first = lung");
}

static void test_stemmer() {
    section("Stemmer");

    struct { const char* input; const char* expected; } cases[] = {
        {"cancer",        "cancer"},
        {"tumors",        "tumor"},
        {"treatments",    "treatment"},
        {"diagnosed",     "diagnos"},
        {"running",       "run"},
        {"happily",       "happili"},
        {"generalization","gener"},
        {"troubles",      "troubl"},
        {"motional",      "motion"},
        {"rational",      "ration"},
    };
    int n = (int)(sizeof(cases) / sizeof(cases[0]));

    for (int i = 0; i < n; i++) {
        char word[128];
        strncpy(word, cases[i].input, sizeof(word) - 1);
        word[sizeof(word) - 1] = '\0';
        porter_stem(word);
        char name[256];
        snprintf(name, sizeof(name), "stem(%s) = %s", cases[i].input, cases[i].expected);
        check(strcmp(word, cases[i].expected) == 0, name);
    }

    char longword[512];
    memset(longword, 'a', sizeof(longword) - 1);
    longword[sizeof(longword) - 1] = '\0';
    porter_stem(longword);
    check(true, "stemmer handles very long word without crash");
}

static void test_compress() {
    section("VByte Compression");

    unsigned char buf[64];

    int n = vbyte_encode(0, buf);
    unsigned int v;
    vbyte_decode(buf, &v);
    check(n == 1 && v == 0, "vbyte 0 -> 1 byte, decode = 0");

    n = vbyte_encode(127, buf);
    vbyte_decode(buf, &v);
    check(n == 1 && v == 127, "vbyte 127 -> 1 byte");

    n = vbyte_encode(128, buf);
    vbyte_decode(buf, &v);
    check(n == 2 && v == 128, "vbyte 128 -> 2 bytes");

    n = vbyte_encode(16383, buf);
    vbyte_decode(buf, &v);
    check(n == 2 && v == 16383, "vbyte 16383 -> 2 bytes");

    n = vbyte_encode(16384, buf);
    vbyte_decode(buf, &v);
    check(n == 3 && v == 16384, "vbyte 16384 -> 3 bytes");

    n = vbyte_encode(0xFFFFFFFF, buf);
    vbyte_decode(buf, &v);
    check(v == 0xFFFFFFFF, "vbyte max uint32 roundtrip");

    unsigned int ids[] = {10, 20, 35, 100, 1000};
    int cnt = 5;
    int enc_len = vbyte_encode_delta(ids, cnt, buf);
    unsigned int decoded[5];
    vbyte_decode_delta(buf, cnt, decoded);
    bool ok = true;
    for (int i = 0; i < cnt; i++) ok = ok && decoded[i] == ids[i];
    check(ok, "delta encode/decode roundtrip");
    check(enc_len < cnt * 4, "delta encoding is shorter than raw uint32");

    unsigned int vals[] = {5, 12, 3, 99, 0};
    int arr_len = vbyte_encode_array(vals, 5, buf);
    unsigned int decoded2[5];
    vbyte_decode_array(buf, 5, decoded2);
    bool ok2 = true;
    for (int i = 0; i < 5; i++) ok2 = ok2 && decoded2[i] == vals[i];
    check(ok2, "array encode/decode roundtrip");
    check(arr_len < 5 * 4, "array encoding shorter than raw");
}

static void test_tfidf() {
    section("TF-IDF");

    check(fabs(compute_tf(0, 100)) < 1e-9, "tf(0, 100) = 0");
    check(fabs(compute_tf(5, 100) - 0.05) < 1e-9, "tf(5,100) = 0.05");
    check(fabs(compute_tf(10, 10) - 1.0) < 1e-9, "tf(10,10) = 1.0");

    check(fabs(compute_idf(100, 0)) < 1e-9, "idf(df=0) = 0");
    double idf = compute_idf(1000, 10);
    check(fabs(idf - log(100.0)) < 1e-9, "idf(1000,10) = ln(100)");
    check(fabs(compute_idf(100, 100)) < 1e-9, "idf(100,100) = 0");

    double tfidf = compute_tfidf(5, 100, 1000, 10);
    check(fabs(tfidf - 0.05 * log(100.0)) < 1e-9, "tfidf value correct");

    ScoredDoc docs[] = {{1, 0.5}, {2, 0.9}, {3, 0.1}, {4, 0.7}};
    sort_by_score(docs, 4);
    check(docs[0].doc_id == 2 && fabs(docs[0].score - 0.9) < 1e-9, "sort: first = doc2 score 0.9");
    check(docs[3].doc_id == 3 && fabs(docs[3].score - 0.1) < 1e-9, "sort: last = doc3 score 0.1");
    check(docs[1].score >= docs[2].score, "sort: descending order");
}

static void test_query_parser() {
    section("QueryParser");

    QueryNode* n = parse_query("cancer");
    check(n != nullptr && n->type == NODE_TERM, "single term: type = TERM");
    check(n != nullptr && strcmp(n->term, "cancer") == 0, "single term: value = cancer");
    free_query(n);

    n = parse_query("breast cancer");
    check(n != nullptr && n->type == NODE_AND, "implicit AND: type = AND");
    check(n != nullptr && n->left != nullptr && n->left->type == NODE_TERM, "AND: left is TERM");
    check(n != nullptr && n->right != nullptr && n->right->type == NODE_TERM, "AND: right is TERM");
    free_query(n);

    n = parse_query("breast && cancer");
    check(n != nullptr && n->type == NODE_AND, "explicit &&: type = AND");
    free_query(n);

    n = parse_query("breast || lung");
    check(n != nullptr && n->type == NODE_OR, "OR: type = OR");
    free_query(n);

    n = parse_query("!benign");
    check(n != nullptr && n->type == NODE_NOT, "NOT: type = NOT");
    check(n != nullptr && n->left != nullptr && n->left->type == NODE_TERM, "NOT: child is TERM");
    free_query(n);

    n = parse_query("(breast || lung) cancer");
    check(n != nullptr && n->type == NODE_AND, "grouped OR AND term");
    check(n != nullptr && n->left != nullptr && n->left->type == NODE_OR, "left is OR group");
    free_query(n);

    n = parse_query("cancer && !benign");
    check(n != nullptr && n->type == NODE_AND, "AND NOT: type AND");
    check(n != nullptr && n->right != nullptr && n->right->type == NODE_NOT, "AND NOT: right is NOT");
    free_query(n);

    n = parse_query("");
    check(n == nullptr, "empty query returns nullptr");
    free_query(n);
}

int main() {
    printf("=== Unit Test Suite ===\n");

    test_dynarray();
    test_hashmap();
    test_str_utils();
    test_tokenizer();
    test_stemmer();
    test_compress();
    test_tfidf();
    test_query_parser();

    printf("\n========================\n");
    printf("Results: %d passed, %d failed\n", passed, failed);
    printf("========================\n");

    return failed > 0 ? 1 : 0;
}
