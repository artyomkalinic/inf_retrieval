#ifndef SEARCH_H
#define SEARCH_H

#include "index_reader.h"
#include "query_parser.h"
#include "tfidf.h"

struct SearchResult {
    unsigned int doc_id;
    double score;
    char* title;
    char* url;
};

struct SearchResults {
    SearchResult* items;
    int count;
    int total_matched;
};

SearchResults execute_search(IndexReader* reader, const char* query, int offset, int limit);
void free_search_results(SearchResults* results);

#endif
