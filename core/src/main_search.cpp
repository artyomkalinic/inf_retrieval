#include "index_reader.h"
#include "search.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <index_file> [--web <query>]\n", argv[0]);
        return 1;
    }

    const char* index_path = argv[1];
    bool web_mode = false;
    const char* web_query = nullptr;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--web") == 0) {
            web_mode = true;
            if (i + 1 < argc) {
                web_query = argv[i + 1];
                i++;
            }
        }
    }

    IndexReader reader;
    if (!reader.open(index_path)) {
        fprintf(stderr, "Error: cannot open index file '%s'\n", index_path);
        return 1;
    }

    fprintf(stderr, "Index loaded: %u terms, %u documents\n",
            reader.getNumTerms(), reader.getNumDocs());

    if (web_mode) {
        char line[4096];
        const char* q = web_query;

        if (q == nullptr) {
            if (fgets(line, sizeof(line), stdin) == nullptr) {
                reader.close();
                return 0;
            }
            int len = (int)strlen(line);
            while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
                line[--len] = '\0';
            }
            q = line;
        }

        SearchResults results = execute_search(&reader, q, 0, 50);
        for (int i = 0; i < results.count; i++) {
            printf("%u\t%.6f\t%s\t%s\n",
                   results.items[i].doc_id,
                   results.items[i].score,
                   results.items[i].title,
                   results.items[i].url);
        }
        free_search_results(&results);
    } else {
        char line[4096];
        while (fgets(line, sizeof(line), stdin) != nullptr) {
            int len = (int)strlen(line);
            while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
                line[--len] = '\0';
            }
            if (len == 0) {
                continue;
            }

            SearchResults results = execute_search(&reader, line, 0, 50);

            for (int i = 0; i < results.count; i++) {
                printf("%u\t%.6f\t%s\t%s\n",
                       results.items[i].doc_id,
                       results.items[i].score,
                       results.items[i].title,
                       results.items[i].url);
            }
            printf("\n");
            fflush(stdout);

            free_search_results(&results);
        }
    }

    reader.close();
    return 0;
}
