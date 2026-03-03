#ifndef TFIDF_H
#define TFIDF_H

#include <cmath>

double compute_tf(int term_freq, int doc_length);
double compute_idf(int num_docs, int doc_freq);
double compute_tfidf(int term_freq, int doc_length, int num_docs, int doc_freq);

struct ScoredDoc {
    unsigned int doc_id;
    double score;
};

void sort_by_score(ScoredDoc* docs, int count);

#endif
