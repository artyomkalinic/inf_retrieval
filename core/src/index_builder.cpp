#include "index_builder.h"
#include "array.h"
#include "hashmap.h"
#include "str_utils.h"
#include "tokenizer.h"
#include "stemmer.h"
#include "compress.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

struct DocMeta {
    char* title;
    char* url;
    unsigned int doc_id;
    unsigned int token_count;
};

struct RawPosting {
    unsigned int term_id;
    unsigned int doc_id;
    unsigned int tf;
};

static unsigned int murmur3(const char* key, int len) {
    unsigned int seed = 42, c1 = 0xcc9e2d51, c2 = 0x1b873593, h = seed;
    int nblocks = len / 4;
    const unsigned char* d = (const unsigned char*)key;
    for (int i = 0; i < nblocks; i++) {
        unsigned int k = (unsigned int)d[i*4] | (unsigned int)d[i*4+1]<<8 |
                         (unsigned int)d[i*4+2]<<16 | (unsigned int)d[i*4+3]<<24;
        k *= c1; k = (k<<15)|(k>>17); k *= c2;
        h ^= k; h = (h<<13)|(h>>19); h = h*5 + 0xe6546b64;
    }
    const unsigned char* tail = d + nblocks*4;
    unsigned int k1 = 0;
    switch(len & 3) {
        case 3: k1 ^= (unsigned int)tail[2]<<16;
        case 2: k1 ^= (unsigned int)tail[1]<<8;
        case 1: k1 ^= (unsigned int)tail[0];
                k1 *= c1; k1 = (k1<<15)|(k1>>17); k1 *= c2; h ^= k1;
    }
    h ^= (unsigned int)len;
    h ^= h>>16; h *= 0x85ebca6b; h ^= h>>13; h *= 0xc2b2ae35; h ^= h>>16;
    return h;
}

static char* read_file(const char* path, long* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) return nullptr;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(sz + 1);
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);
    *out_size = sz;
    return buf;
}

static int read_line(const char* buf, long bsz, long off, char* out, int maxlen) {
    int i = 0, w = 0;
    while (off + i < bsz && buf[off+i] != '\n' && buf[off+i] != '\r') {
        if (w < maxlen - 1) out[w++] = buf[off+i];
        i++;
    }
    out[w] = '\0';
    if (off+i < bsz && buf[off+i] == '\r') i++;
    if (off+i < bsz && buf[off+i] == '\n') i++;
    return i;
}

static int cmp_raw_posting(const void* a, const void* b) {
    const RawPosting* pa = (const RawPosting*)a;
    const RawPosting* pb = (const RawPosting*)b;
    if (pa->term_id != pb->term_id) return (pa->term_id < pb->term_id) ? -1 : 1;
    if (pa->doc_id != pb->doc_id)   return (pa->doc_id < pb->doc_id) ? -1 : 1;
    return 0;
}

int build_index(const char* corpus_dir, const char* output_path, bool use_compression) {
    clock_t t0 = clock();

    char mpath[1024];
    snprintf(mpath, sizeof(mpath), "%s/manifest.tsv", corpus_dir);
    FILE* mf = fopen(mpath, "r");
    if (!mf) { printf("Error: cannot open %s\n", mpath); return -1; }

    int id_cap = 40000, id_count = 0;
    unsigned int* ids = (unsigned int*)malloc(id_cap * sizeof(unsigned int));
    char line[8192];
    while (fgets(line, sizeof(line), mf)) {
        if (!is_digit(line[0])) continue;
        unsigned int did = 0;
        for (int i = 0; line[i] != '\t' && line[i] != '\0' && line[i] != '\n'; i++)
            if (is_digit(line[i])) did = did * 10 + (line[i] - '0');
        if (id_count >= id_cap) { id_cap *= 2; ids = (unsigned int*)realloc(ids, id_cap * sizeof(unsigned int)); }
        ids[id_count++] = did;
    }
    fclose(mf);
    printf("Manifest: %d documents\n", id_count); fflush(stdout);

    HashMap terms(524288);
    int tnames_cap = 200000, tnames_count = 0;
    char** tnames = (char**)malloc(tnames_cap * sizeof(char*));

    int post_cap = 8000000, post_count = 0;
    RawPosting* posts = (RawPosting*)malloc(post_cap * sizeof(RawPosting));

    int docs_count = 0;
    DocMeta* docs = (DocMeta*)malloc((id_count + 1) * sizeof(DocMeta));

    for (int d = 0; d < id_count; d++) {
        char fp[1024];
        snprintf(fp, sizeof(fp), "%s/%u.txt", corpus_dir, ids[d]);
        long fsz = 0;
        char* fb = read_file(fp, &fsz);
        if (!fb) continue;

        char title[1024] = {0}, url[2048] = {0};
        long off = 0;
        off += read_line(fb, fsz, off, title, sizeof(title));
        off += read_line(fb, fsz, off, url, sizeof(url));
        long blen = fsz - off; if (blen < 0) blen = 0;

        std::string body(fb + off, blen);
        std::vector<std::string> toks = tokenize_simple(body);
        free(fb);

        HashMap dtf(1024);
        unsigned int ntoks = (unsigned int)toks.size();
        for (int t = 0; t < (int)toks.size(); t++) {
            char tb[512];
            str_copy(tb, toks[t].c_str(), sizeof(tb));
            str_to_lower(tb); porter_stem(tb);
            if (str_len(tb) == 0) continue;
            unsigned int c = 0;
            if (dtf.get(tb, &c)) dtf.put(tb, c + 1);
            else dtf.put(tb, 1);
        }

        for (int b = 0; b < dtf.capacity; b++) {
            if (dtf.buckets[b].flags != 1) continue;
            const char* term = dtf.buckets[b].key;
            unsigned int tf = dtf.buckets[b].value;

            unsigned int tid;
            if (!terms.get(term, &tid)) {
                tid = (unsigned int)tnames_count;
                terms.put(term, tid);
                if (tnames_count >= tnames_cap) {
                    tnames_cap *= 2;
                    tnames = (char**)realloc(tnames, tnames_cap * sizeof(char*));
                }
                tnames[tnames_count++] = str_dup(term);
            }

            if (post_count >= post_cap) {
                post_cap *= 2;
                RawPosting* newp = (RawPosting*)realloc(posts, (size_t)post_cap * sizeof(RawPosting));
                if (!newp) { fprintf(stderr, "FATAL: realloc posts failed at %d\n", post_cap); abort(); }
                posts = newp;
            }
            posts[post_count].term_id = tid;
            posts[post_count].doc_id = ids[d];
            posts[post_count].tf = tf;
            post_count++;
        }

        docs[docs_count].title = str_dup(title);
        docs[docs_count].url = str_dup(url);
        docs[docs_count].doc_id = ids[d];
        docs[docs_count].token_count = ntoks;
        docs_count++;

        if ((d+1) % 2000 == 0) {
            printf("  %d / %d docs (terms=%d, posts=%d)\n",
                   d+1, id_count, tnames_count, post_count);
            fflush(stdout);
        }
    }
    free(ids);

    int num_terms = tnames_count;
    int num_docs = docs_count;
    printf("Done: %d terms, %d docs, %d postings\n", num_terms, num_docs, post_count);
    fflush(stdout);

    printf("Sorting postings...\n"); fflush(stdout);
    qsort(posts, post_count, sizeof(RawPosting), cmp_raw_posting);
    printf("Sorted.\n"); fflush(stdout);

    int* tstart = (int*)malloc(num_terms * sizeof(int));
    int* tcount = (int*)calloc(num_terms, sizeof(int));
    {
        int cur_term = -1;
        for (int i = 0; i < post_count; i++) {
            int tid = (int)posts[i].term_id;
            if (tid != cur_term) { tstart[tid] = i; cur_term = tid; }
            tcount[tid]++;
        }
    }

    unsigned int spool_sz = 0;
    unsigned int* t_soff = (unsigned int*)malloc(num_terms * sizeof(unsigned int));
    unsigned short* t_slen = (unsigned short*)malloc(num_terms * sizeof(unsigned short));
    for (int t = 0; t < num_terms; t++) {
        t_soff[t] = spool_sz;
        t_slen[t] = (unsigned short)str_len(tnames[t]);
        spool_sz += t_slen[t] + 1;
    }
    unsigned int* d_toff = (unsigned int*)malloc(num_docs * sizeof(unsigned int));
    unsigned short* d_tlen = (unsigned short*)malloc(num_docs * sizeof(unsigned short));
    unsigned int* d_uoff = (unsigned int*)malloc(num_docs * sizeof(unsigned int));
    unsigned short* d_ulen = (unsigned short*)malloc(num_docs * sizeof(unsigned short));
    for (int d = 0; d < num_docs; d++) {
        d_toff[d] = spool_sz; d_tlen[d] = (unsigned short)str_len(docs[d].title); spool_sz += d_tlen[d]+1;
        d_uoff[d] = spool_sz; d_ulen[d] = (unsigned short)str_len(docs[d].url);   spool_sz += d_ulen[d]+1;
    }
    char* spool = (char*)calloc(spool_sz + 1, 1);
    for (int t = 0; t < num_terms; t++) memcpy(spool + t_soff[t], tnames[t], t_slen[t]);
    for (int d = 0; d < num_docs; d++) {
        memcpy(spool + d_toff[d], docs[d].title, d_tlen[d]);
        memcpy(spool + d_uoff[d], docs[d].url, d_ulen[d]);
    }

    unsigned int* p_boff = (unsigned int*)malloc(num_terms * sizeof(unsigned int));
    unsigned char* cbuf = nullptr;
    unsigned int ctotal = 0, raw_bytes = 0;

    if (use_compression) {
        cbuf = (unsigned char*)malloc((long long)post_count * 10 + 4096);
        for (int t = 0; t < num_terms; t++) {
            p_boff[t] = ctotal;
            int cnt = tcount[t], st = tstart[t];
            unsigned int* di = (unsigned int*)malloc(cnt * sizeof(unsigned int));
            unsigned int* tf = (unsigned int*)malloc(cnt * sizeof(unsigned int));
            for (int j = 0; j < cnt; j++) { di[j] = posts[st+j].doc_id; tf[j] = posts[st+j].tf; }
            ctotal += vbyte_encode_delta(di, cnt, cbuf + ctotal);
            ctotal += vbyte_encode_array(tf, cnt, cbuf + ctotal);
            free(di); free(tf);
        }
    } else {
        for (int t = 0; t < num_terms; t++) {
            p_boff[t] = raw_bytes;
            raw_bytes += tcount[t] * (unsigned int)sizeof(PostingEntry);
        }
    }

    FILE* out = fopen(output_path, "wb");
    if (!out) { printf("Error: cannot create %s\n", output_path); return -1; }

    IndexHeader hdr; memset(&hdr, 0, sizeof(hdr));
    hdr.magic = INDEX_MAGIC; hdr.version = INDEX_VERSION;
    hdr.num_terms = (unsigned int)num_terms; hdr.num_docs = (unsigned int)num_docs;
    fwrite(&hdr, sizeof(hdr), 1, out);

    hdr.hash_table_offset = (unsigned long long)ftell(out);
    unsigned long long hcap = (num_terms > 0) ? (unsigned long long)num_terms * 2 : 2;
    hdr.hash_table_slots = hcap;
    HashSlot* ht = (HashSlot*)calloc((size_t)hcap, sizeof(HashSlot));
    for (int t = 0; t < num_terms; t++) {
        int tl = str_len(tnames[t]);
        unsigned long long s = murmur3(tnames[t], tl) % hcap;
        while (ht[s].flags) s = (s+1) % hcap;
        ht[s].term_str_off = t_soff[t]; ht[s].term_str_len = t_slen[t];
        ht[s].post_off = p_boff[t]; ht[s].post_count = (unsigned int)tcount[t];
        ht[s].flags = use_compression ? 2 : 1;
    }
    fwrite(ht, sizeof(HashSlot), (size_t)hcap, out);
    free(ht);

    hdr.postings_offset = (unsigned long long)ftell(out);
    if (use_compression) {
        fwrite(cbuf, 1, ctotal, out); free(cbuf);
    } else {
        for (int t = 0; t < num_terms; t++) {
            int st = tstart[t], cnt = tcount[t];
            for (int j = 0; j < cnt; j++) {
                PostingEntry pe; pe.doc_id = posts[st+j].doc_id; pe.tf = posts[st+j].tf;
                fwrite(&pe, sizeof(pe), 1, out);
            }
        }
    }

    hdr.forward_offset = (unsigned long long)ftell(out);
    for (int d = 0; d < num_docs; d++) {
        ForwardEntry fe; memset(&fe, 0, sizeof(fe));
        fe.title_str_off = d_toff[d]; fe.title_str_len = d_tlen[d];
        fe.url_str_off = d_uoff[d]; fe.url_str_len = d_ulen[d];
        fe.doc_total_tokens = docs[d].token_count;
        fwrite(&fe, sizeof(fe), 1, out);
    }

    hdr.strings_offset = (unsigned long long)ftell(out);
    fwrite(spool, 1, spool_sz, out);

    fseek(out, 0, SEEK_SET);
    fwrite(&hdr, sizeof(hdr), 1, out);
    fseek(out, 0, SEEK_END);
    long idx_size = ftell(out);
    fclose(out);

    for (int t = 0; t < num_terms; t++) free(tnames[t]);
    free(tnames); free(t_soff); free(t_slen);
    for (int d = 0; d < num_docs; d++) { free(docs[d].title); free(docs[d].url); }
    free(docs); free(d_toff); free(d_tlen); free(d_uoff); free(d_ulen);
    free(spool); free(posts); free(tstart); free(tcount); free(p_boff);

    double elapsed = (double)(clock() - t0) / CLOCKS_PER_SEC;
    printf("\n=== Index Build Statistics ===\n");
    printf("Terms:       %d\n", num_terms);
    printf("Documents:   %d\n", num_docs);
    printf("Index size:  %ld bytes (%.2f MB)\n", idx_size, idx_size / (1024.0*1024.0));
    printf("Build time:  %.2f seconds\n", elapsed);
    printf("Compression: %s\n", use_compression ? "VByte" : "none");
    fflush(stdout);
    return 0;
}
