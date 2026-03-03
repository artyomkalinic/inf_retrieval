#include "index_reader.h"
#include "compress.h"
#include "str_utils.h"

#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

IndexReader::IndexReader() {
    mapped_data = nullptr;
    file_size = 0;
    fd = -1;
    header = nullptr;
    hash_table = nullptr;
    postings_base = nullptr;
    forward_base = nullptr;
    strings_base = nullptr;
}

IndexReader::~IndexReader() {
    close();
}

bool IndexReader::open(const char* path) {
    fd = ::open(path, O_RDONLY);
    if (fd < 0) {
        return false;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        ::close(fd);
        fd = -1;
        return false;
    }

    file_size = st.st_size;

    mapped_data = (char*)mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped_data == MAP_FAILED) {
        mapped_data = nullptr;
        ::close(fd);
        fd = -1;
        return false;
    }

    header = (IndexHeader*)mapped_data;

    if (header->magic != INDEX_MAGIC) {
        close();
        return false;
    }

    hash_table = (HashSlot*)(mapped_data + header->hash_table_offset);
    postings_base = mapped_data + header->postings_offset;
    forward_base = (ForwardEntry*)(mapped_data + header->forward_offset);
    strings_base = mapped_data + header->strings_offset;

    return true;
}

void IndexReader::close() {
    if (mapped_data != nullptr) {
        munmap(mapped_data, file_size);
        mapped_data = nullptr;
    }
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
    header = nullptr;
    hash_table = nullptr;
    postings_base = nullptr;
    forward_base = nullptr;
    strings_base = nullptr;
}

unsigned int IndexReader::murmur3(const char* key, int len) {
    unsigned int seed = 42;
    unsigned int c1 = 0xcc9e2d51;
    unsigned int c2 = 0x1b873593;
    unsigned int h = seed;

    int nblocks = len / 4;
    const unsigned char* data = (const unsigned char*)key;

    for (int i = 0; i < nblocks; i++) {
        unsigned int k =
            (unsigned int)data[i * 4 + 0]       |
            (unsigned int)data[i * 4 + 1] << 8  |
            (unsigned int)data[i * 4 + 2] << 16 |
            (unsigned int)data[i * 4 + 3] << 24;

        k *= c1;
        k = (k << 15) | (k >> 17);
        k *= c2;

        h ^= k;
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xe6546b64;
    }

    const unsigned char* tail = data + nblocks * 4;
    unsigned int k1 = 0;

    switch (len & 3) {
        case 3: k1 ^= (unsigned int)tail[2] << 16;
        case 2: k1 ^= (unsigned int)tail[1] << 8;
        case 1: k1 ^= (unsigned int)tail[0];
                k1 *= c1;
                k1 = (k1 << 15) | (k1 >> 17);
                k1 *= c2;
                h ^= k1;
    }

    h ^= (unsigned int)len;

    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;
}

int IndexReader::findTermSlot(const char* term, int len) {
    unsigned int hash = murmur3(term, len);
    int slots = (int)header->hash_table_slots;
    int idx = hash % slots;

    for (int i = 0; i < slots; i++) {
        int slot = (idx + i) % slots;
        HashSlot* hs = &hash_table[slot];

        if (hs->flags == 0) {
            return -1;
        }

        if (hs->term_str_len == (unsigned short)len) {
            const char* stored = strings_base + hs->term_str_off;
            if (memcmp(stored, term, len) == 0) {
                return slot;
            }
        }
    }

    return -1;
}

PostingList IndexReader::getPostings(const char* term) {
    PostingList pl;
    pl.doc_ids = nullptr;
    pl.tfs = nullptr;
    pl.count = 0;

    int len = str_len(term);
    int slot = findTermSlot(term, len);
    if (slot < 0) {
        return pl;
    }

    HashSlot* hs = &hash_table[slot];
    int count = (int)hs->post_count;
    if (count == 0) {
        return pl;
    }

    pl.doc_ids = (unsigned int*)malloc(count * sizeof(unsigned int));
    pl.tfs = (unsigned int*)malloc(count * sizeof(unsigned int));
    pl.count = count;

    bool compressed = (hs->flags == 2);

    if (compressed) {
        const unsigned char* data = (const unsigned char*)(postings_base + hs->post_off);
        int bytes_read = vbyte_decode_delta(data, count, pl.doc_ids);
        vbyte_decode_array(data + bytes_read, count, pl.tfs);
    } else {
        PostingEntry* entries = (PostingEntry*)(postings_base + hs->post_off);
        for (int i = 0; i < count; i++) {
            pl.doc_ids[i] = entries[i].doc_id;
            pl.tfs[i] = entries[i].tf;
        }
    }

    return pl;
}

DocInfo IndexReader::getDocInfo(unsigned int doc_id) {
    DocInfo di;
    di.title = nullptr;
    di.url = nullptr;
    di.total_tokens = 0;

    if (doc_id >= header->num_docs) {
        return di;
    }

    ForwardEntry* fe = &forward_base[doc_id];

    char* title = (char*)malloc(fe->title_str_len + 1);
    memcpy(title, strings_base + fe->title_str_off, fe->title_str_len);
    title[fe->title_str_len] = '\0';
    di.title = title;

    char* url = (char*)malloc(fe->url_str_len + 1);
    memcpy(url, strings_base + fe->url_str_off, fe->url_str_len);
    url[fe->url_str_len] = '\0';
    di.url = url;

    di.total_tokens = fe->doc_total_tokens;

    return di;
}

unsigned int IndexReader::getNumDocs() {
    if (header == nullptr) return 0;
    return header->num_docs;
}

unsigned int IndexReader::getNumTerms() {
    if (header == nullptr) return 0;
    return header->num_terms;
}

void free_posting_list(PostingList* pl) {
    if (pl->doc_ids != nullptr) {
        free(pl->doc_ids);
        pl->doc_ids = nullptr;
    }
    if (pl->tfs != nullptr) {
        free(pl->tfs);
        pl->tfs = nullptr;
    }
    pl->count = 0;
}

void free_doc_info(DocInfo* di) {
    if (di->title != nullptr) {
        free(di->title);
        di->title = nullptr;
    }
    if (di->url != nullptr) {
        free(di->url);
        di->url = nullptr;
    }
}
