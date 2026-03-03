#include "search.h"
#include "stemmer.h"
#include "str_utils.h"

#include <cstdlib>
#include <cstring>

static PostingList empty_posting_list() {
    PostingList pl;
    pl.doc_ids = nullptr;
    pl.tfs = nullptr;
    pl.count = 0;
    return pl;
}

static char* prepare_term(const char* raw_term) {
    char* term = str_dup(raw_term);
    str_to_lower(term);
    porter_stem(term);
    return term;
}

static PostingList intersect_postings(PostingList* a, PostingList* b) {
    PostingList result;
    int max_count = (a->count < b->count) ? a->count : b->count;
    if (max_count == 0) {
        return empty_posting_list();
    }

    result.doc_ids = (unsigned int*)malloc(max_count * sizeof(unsigned int));
    result.tfs = (unsigned int*)malloc(max_count * sizeof(unsigned int));
    result.count = 0;

    int i = 0, j = 0;
    while (i < a->count && j < b->count) {
        if (a->doc_ids[i] == b->doc_ids[j]) {
            result.doc_ids[result.count] = a->doc_ids[i];
            result.tfs[result.count] = a->tfs[i] + b->tfs[j];
            result.count++;
            i++;
            j++;
        } else if (a->doc_ids[i] < b->doc_ids[j]) {
            i++;
        } else {
            j++;
        }
    }

    return result;
}

static PostingList union_postings(PostingList* a, PostingList* b) {
    PostingList result;
    int max_count = a->count + b->count;
    if (max_count == 0) {
        return empty_posting_list();
    }

    result.doc_ids = (unsigned int*)malloc(max_count * sizeof(unsigned int));
    result.tfs = (unsigned int*)malloc(max_count * sizeof(unsigned int));
    result.count = 0;

    int i = 0, j = 0;
    while (i < a->count && j < b->count) {
        if (a->doc_ids[i] == b->doc_ids[j]) {
            result.doc_ids[result.count] = a->doc_ids[i];
            result.tfs[result.count] = a->tfs[i] + b->tfs[j];
            result.count++;
            i++;
            j++;
        } else if (a->doc_ids[i] < b->doc_ids[j]) {
            result.doc_ids[result.count] = a->doc_ids[i];
            result.tfs[result.count] = a->tfs[i];
            result.count++;
            i++;
        } else {
            result.doc_ids[result.count] = b->doc_ids[j];
            result.tfs[result.count] = b->tfs[j];
            result.count++;
            j++;
        }
    }

    while (i < a->count) {
        result.doc_ids[result.count] = a->doc_ids[i];
        result.tfs[result.count] = a->tfs[i];
        result.count++;
        i++;
    }

    while (j < b->count) {
        result.doc_ids[result.count] = b->doc_ids[j];
        result.tfs[result.count] = b->tfs[j];
        result.count++;
        j++;
    }

    return result;
}

static PostingList subtract_postings(PostingList* a, PostingList* b) {
    if (a->count == 0) {
        return empty_posting_list();
    }

    PostingList result;
    result.doc_ids = (unsigned int*)malloc(a->count * sizeof(unsigned int));
    result.tfs = (unsigned int*)malloc(a->count * sizeof(unsigned int));
    result.count = 0;

    int i = 0, j = 0;
    while (i < a->count) {
        if (j < b->count) {
            if (a->doc_ids[i] < b->doc_ids[j]) {
                result.doc_ids[result.count] = a->doc_ids[i];
                result.tfs[result.count] = a->tfs[i];
                result.count++;
                i++;
            } else if (a->doc_ids[i] == b->doc_ids[j]) {
                i++;
                j++;
            } else {
                j++;
            }
        } else {
            result.doc_ids[result.count] = a->doc_ids[i];
            result.tfs[result.count] = a->tfs[i];
            result.count++;
            i++;
        }
    }

    return result;
}

static PostingList evaluate_node(IndexReader* reader, QueryNode* node) {
    if (node == nullptr) {
        return empty_posting_list();
    }

    if (node->type == NODE_TERM) {
        char* processed = prepare_term(node->term);
        PostingList pl = reader->getPostings(processed);
        free(processed);
        return pl;
    }

    if (node->type == NODE_AND) {
        bool left_is_not = (node->left != nullptr && node->left->type == NODE_NOT);
        bool right_is_not = (node->right != nullptr && node->right->type == NODE_NOT);

        if (left_is_not && right_is_not) {
            return empty_posting_list();
        }

        if (left_is_not) {
            PostingList right = evaluate_node(reader, node->right);
            PostingList neg = evaluate_node(reader, node->left->left);
            PostingList result = subtract_postings(&right, &neg);
            free_posting_list(&right);
            free_posting_list(&neg);
            return result;
        }

        if (right_is_not) {
            PostingList left = evaluate_node(reader, node->left);
            PostingList neg = evaluate_node(reader, node->right->left);
            PostingList result = subtract_postings(&left, &neg);
            free_posting_list(&left);
            free_posting_list(&neg);
            return result;
        }

        PostingList left = evaluate_node(reader, node->left);
        PostingList right = evaluate_node(reader, node->right);
        PostingList result = intersect_postings(&left, &right);
        free_posting_list(&left);
        free_posting_list(&right);
        return result;
    }

    if (node->type == NODE_OR) {
        PostingList left = evaluate_node(reader, node->left);
        PostingList right = evaluate_node(reader, node->right);
        PostingList result = union_postings(&left, &right);
        free_posting_list(&left);
        free_posting_list(&right);
        return result;
    }

    return empty_posting_list();
}

static void collect_terms(QueryNode* node, char*** terms, int* count, int* cap) {
    if (node == nullptr) return;
    if (node->type == NODE_NOT) return;

    if (node->type == NODE_TERM) {
        char* processed = prepare_term(node->term);

        for (int i = 0; i < *count; i++) {
            if (str_cmp((*terms)[i], processed) == 0) {
                free(processed);
                return;
            }
        }

        if (*count == *cap) {
            *cap *= 2;
            *terms = (char**)realloc(*terms, *cap * sizeof(char*));
        }
        (*terms)[*count] = processed;
        (*count)++;
        return;
    }

    collect_terms(node->left, terms, count, cap);
    collect_terms(node->right, terms, count, cap);
}

static int find_tf_in_postings(PostingList* pl, unsigned int doc_id) {
    int lo = 0, hi = pl->count - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (pl->doc_ids[mid] == doc_id) {
            return (int)pl->tfs[mid];
        }
        if (pl->doc_ids[mid] < doc_id) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return 0;
}

SearchResults execute_search(IndexReader* reader, const char* query,
                             int offset, int limit) {
    SearchResults results;
    results.items = nullptr;
    results.count = 0;
    results.total_matched = 0;

    QueryNode* root = parse_query(query);
    if (root == nullptr) {
        return results;
    }

    PostingList matched = evaluate_node(reader, root);
    results.total_matched = matched.count;

    if (matched.count == 0) {
        free_query(root);
        free_posting_list(&matched);
        return results;
    }

    int term_count = 0;
    int term_cap = 16;
    char** terms = (char**)malloc(term_cap * sizeof(char*));
    collect_terms(root, &terms, &term_count, &term_cap);

    PostingList* term_pl = (PostingList*)malloc(term_count * sizeof(PostingList));
    for (int i = 0; i < term_count; i++) {
        term_pl[i] = reader->getPostings(terms[i]);
    }

    unsigned int num_docs = reader->getNumDocs();

    ScoredDoc* scored = (ScoredDoc*)malloc(matched.count * sizeof(ScoredDoc));
    for (int i = 0; i < matched.count; i++) {
        unsigned int doc_id = matched.doc_ids[i];
        DocInfo di = reader->getDocInfo(doc_id);
        double score = 0.0;

        for (int t = 0; t < term_count; t++) {
            int tf = find_tf_in_postings(&term_pl[t], doc_id);
            if (tf > 0) {
                score += compute_tfidf(tf, (int)di.total_tokens,
                                       (int)num_docs, term_pl[t].count);
            }
        }

        scored[i].doc_id = doc_id;
        scored[i].score = score;
        free_doc_info(&di);
    }

    sort_by_score(scored, matched.count);

    int start = offset;
    if (start > matched.count) start = matched.count;
    int end = start + limit;
    if (end > matched.count) end = matched.count;
    int result_count = end - start;

    if (result_count > 0) {
        results.items = (SearchResult*)malloc(result_count * sizeof(SearchResult));
        results.count = result_count;

        for (int i = 0; i < result_count; i++) {
            unsigned int doc_id = scored[start + i].doc_id;
            DocInfo di = reader->getDocInfo(doc_id);
            results.items[i].doc_id = doc_id;
            results.items[i].score = scored[start + i].score;
            results.items[i].title = di.title;
            results.items[i].url = di.url;
        }
    }

    free(scored);
    for (int i = 0; i < term_count; i++) {
        free_posting_list(&term_pl[i]);
        free(terms[i]);
    }
    free(term_pl);
    free(terms);
    free_posting_list(&matched);
    free_query(root);

    return results;
}

void free_search_results(SearchResults* results) {
    if (results->items != nullptr) {
        for (int i = 0; i < results->count; i++) {
            if (results->items[i].title != nullptr) {
                free(results->items[i].title);
            }
            if (results->items[i].url != nullptr) {
                free(results->items[i].url);
            }
        }
        free(results->items);
        results->items = nullptr;
    }
    results->count = 0;
    results->total_matched = 0;
}
