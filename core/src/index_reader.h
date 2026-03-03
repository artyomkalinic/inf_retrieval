#ifndef INDEX_READER_H
#define INDEX_READER_H

#include "index_builder.h"

struct PostingList {
    unsigned int* doc_ids;
    unsigned int* tfs;
    int count;
};

struct DocInfo {
    char* title;
    char* url;
    unsigned int total_tokens;
};

class IndexReader {
public:
    char* mapped_data;
    long long file_size;
    int fd;

    IndexHeader* header;
    HashSlot* hash_table;
    char* postings_base;
    ForwardEntry* forward_base;
    char* strings_base;

    IndexReader();
    ~IndexReader();

    bool open(const char* path);
    void close();

    PostingList getPostings(const char* term);
    DocInfo getDocInfo(unsigned int doc_id);
    unsigned int getNumDocs();
    unsigned int getNumTerms();

private:
    int findTermSlot(const char* term, int len);
    unsigned int murmur3(const char* key, int len);
};

void free_posting_list(PostingList* pl);
void free_doc_info(DocInfo* di);

#endif
