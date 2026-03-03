#ifndef INDEX_BUILDER_H
#define INDEX_BUILDER_H

#define INDEX_MAGIC 0x48435253  // "SRCH" in little-endian
#define INDEX_VERSION 2

struct IndexHeader {
    unsigned int magic;
    unsigned int version;
    unsigned int num_terms;
    unsigned int num_docs;
    unsigned long long hash_table_offset;
    unsigned long long hash_table_slots;
    unsigned long long postings_offset;
    unsigned long long forward_offset;
    unsigned long long strings_offset;
    unsigned long long reserved;
};

struct HashSlot {
    unsigned int term_str_off;
    unsigned short term_str_len;
    unsigned int post_off;
    unsigned int post_count;
    unsigned short flags;
};

struct PostingEntry {
    unsigned int doc_id;
    unsigned int tf;
};

struct ForwardEntry {
    unsigned int title_str_off;
    unsigned short title_str_len;
    unsigned int url_str_off;
    unsigned short url_str_len;
    unsigned int doc_total_tokens;
};

int build_index(const char* corpus_dir, const char* output_path, bool use_compression);

#endif
