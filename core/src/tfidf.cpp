#include "tfidf.h"

double compute_tf(int term_freq, int doc_length) {
    if (doc_length == 0) {
        return 0.0;
    }
    return (double)term_freq / (double)doc_length;
}

double compute_idf(int num_docs, int doc_freq) {
    if (doc_freq == 0) {
        return 0.0;
    }
    return log((double)num_docs / (double)doc_freq);
}

double compute_tfidf(int term_freq, int doc_length, int num_docs, int doc_freq) {
    return compute_tf(term_freq, doc_length) * compute_idf(num_docs, doc_freq);
}

static void swap_scored(ScoredDoc* a, ScoredDoc* b) {
    ScoredDoc tmp = *a;
    *a = *b;
    *b = tmp;
}

static int partition(ScoredDoc* docs, int lo, int hi) {
    double pivot = docs[lo + (hi - lo) / 2].score;
    int i = lo;
    int j = hi;

    while (i <= j) {
        while (docs[i].score > pivot) {
            i++;
        }
        while (docs[j].score < pivot) {
            j--;
        }
        if (i <= j) {
            swap_scored(&docs[i], &docs[j]);
            i++;
            j--;
        }
    }
    return i;
}

static void quicksort_desc(ScoredDoc* docs, int lo, int hi) {
    if (lo >= hi) {
        return;
    }
    int p = partition(docs, lo, hi);
    quicksort_desc(docs, lo, p - 1);
    quicksort_desc(docs, p, hi);
}

void sort_by_score(ScoredDoc* docs, int count) {
    if (count <= 1) {
        return;
    }
    quicksort_desc(docs, 0, count - 1);
}
