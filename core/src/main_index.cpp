#include "index_builder.h"

#include <cstdio>
#include <cstring>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <corpus_dir> <output_index_file> [--compress]\n",
                argv[0]);
        return 1;
    }

    const char* corpus_dir = argv[1];
    const char* output_path = argv[2];
    bool use_compression = false;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--compress") == 0) {
            use_compression = true;
        }
    }

    fprintf(stderr, "Building index from '%s' -> '%s'%s\n",
            corpus_dir, output_path,
            use_compression ? " (compressed)" : "");

    int result = build_index(corpus_dir, output_path, use_compression);

    if (result == 0) {
        fprintf(stderr, "Index built successfully.\n");
    } else {
        fprintf(stderr, "Error: failed to build index (code %d)\n", result);
    }

    return result;
}
