#include "stemmer.h"
#include <cstring>

static bool is_consonant(const char* w, int i) {
    char c = w[i];
    if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u') {
        return false;
    }
    if (c == 'y') {
        if (i == 0) {
            return true;
        }
        return !is_consonant(w, i - 1);
    }
    return true;
}

static int measure(const char* w) {
    int len = (int)strlen(w);
    int m = 0;
    int i = 0;

    while (i < len && is_consonant(w, i)) {
        i++;
    }

    while (i < len) {
        while (i < len && !is_consonant(w, i)) {
            i++;
        }
        if (i >= len) {
            break;
        }
        while (i < len && is_consonant(w, i)) {
            i++;
        }
        m++;
    }
    return m;
}

static bool has_vowel(const char* w) {
    int len = (int)strlen(w);
    for (int i = 0; i < len; i++) {
        if (!is_consonant(w, i)) {
            return true;
        }
    }
    return false;
}

static bool ends_with(const char* w, const char* suffix) {
    int wlen = (int)strlen(w);
    int slen = (int)strlen(suffix);
    if (slen > wlen) {
        return false;
    }
    return strcmp(w + wlen - slen, suffix) == 0;
}

static bool ends_double_consonant(const char* w) {
    int len = (int)strlen(w);
    if (len < 2) {
        return false;
    }
    if (w[len - 1] != w[len - 2]) {
        return false;
    }
    return is_consonant(w, len - 1);
}

static bool ends_cvc(const char* w) {
    int len = (int)strlen(w);
    if (len < 3) {
        return false;
    }
    int j = len - 1;
    if (!is_consonant(w, j) || is_consonant(w, j - 1) || !is_consonant(w, j - 2)) {
        return false;
    }
    char c = w[j];
    if (c == 'w' || c == 'x' || c == 'y') {
        return false;
    }
    return true;
}

static void step1a(char* w) {
    if (ends_with(w, "sses")) {
        w[strlen(w) - 2] = '\0';
    } else if (ends_with(w, "ies")) {
        w[strlen(w) - 2] = '\0';
    } else if (!ends_with(w, "ss") && ends_with(w, "s")) {
        w[strlen(w) - 1] = '\0';
    }
}

static void step1b(char* w) {
    int len = (int)strlen(w);

    if (ends_with(w, "eed")) {
        w[len - 1] = '\0';
        if (measure(w) <= 0) {
            w[len - 1] = 'd';
        }
        return;
    }

    bool trimmed = false;

    if (ends_with(w, "ed")) {
        w[len - 2] = '\0';
        if (has_vowel(w)) {
            trimmed = true;
        } else {
            w[len - 2] = 'e';
            w[len - 1] = 'd';
        }
    } else if (ends_with(w, "ing")) {
        w[len - 3] = '\0';
        if (has_vowel(w)) {
            trimmed = true;
        } else {
            w[len - 3] = 'i';
            w[len - 2] = 'n';
            w[len - 1] = 'g';
        }
    }

    if (trimmed) {
        if (ends_with(w, "at") || ends_with(w, "bl") || ends_with(w, "iz")) {
            int newlen = (int)strlen(w);
            w[newlen] = 'e';
            w[newlen + 1] = '\0';
        } else if (ends_double_consonant(w)) {
            int newlen = (int)strlen(w);
            char last = w[newlen - 1];
            if (last != 'l' && last != 's' && last != 'z') {
                w[newlen - 1] = '\0';
            }
        } else if (measure(w) == 1 && ends_cvc(w)) {
            int newlen = (int)strlen(w);
            w[newlen] = 'e';
            w[newlen + 1] = '\0';
        }
    }
}

static void step1c(char* w) {
    int len = (int)strlen(w);
    if (len > 1 && w[len - 1] == 'y') {
        w[len - 1] = '\0';
        if (has_vowel(w)) {
            w[len - 1] = 'i';
        } else {
            w[len - 1] = 'y';
        }
    }
}

static void step2(char* w) {
    struct Rule {
        const char* suffix;
        const char* replacement;
    };
    Rule rules[] = {
        {"ational", "ate"},
        {"tional",  "tion"},
        {"enci",    "ence"},
        {"anci",    "ance"},
        {"izer",    "ize"},
        {"abli",    "able"},
        {"alli",    "al"},
        {"entli",   "ent"},
        {"eli",     "e"},
        {"ousli",   "ous"},
        {"ization", "ize"},
        {"ation",   "ate"},
        {"ator",    "ate"},
        {"alism",   "al"},
        {"iveness", "ive"},
        {"fulness", "ful"},
        {"ousness", "ous"},
        {"aliti",   "al"},
        {"iviti",   "ive"},
        {"biliti",  "ble"},
    };
    int num_rules = (int)(sizeof(rules) / sizeof(rules[0]));

    for (int i = 0; i < num_rules; i++) {
        if (ends_with(w, rules[i].suffix)) {
            int wlen = (int)strlen(w);
            int slen = (int)strlen(rules[i].suffix);
            char saved[512];
            strcpy(saved, w);
            w[wlen - slen] = '\0';
            if (measure(w) > 0) {
                strcat(w, rules[i].replacement);
            } else {
                strcpy(w, saved);
            }
            return;
        }
    }
}

static void step3(char* w) {
    struct Rule {
        const char* suffix;
        const char* replacement;
    };
    Rule rules[] = {
        {"icate", "ic"},
        {"ative", ""},
        {"alize", "al"},
        {"iciti", "ic"},
        {"ical",  "ic"},
        {"ful",   ""},
        {"ness",  ""},
    };
    int num_rules = (int)(sizeof(rules) / sizeof(rules[0]));

    for (int i = 0; i < num_rules; i++) {
        if (ends_with(w, rules[i].suffix)) {
            int wlen = (int)strlen(w);
            int slen = (int)strlen(rules[i].suffix);
            char saved[512];
            strcpy(saved, w);
            w[wlen - slen] = '\0';
            if (measure(w) > 0) {
                strcat(w, rules[i].replacement);
            } else {
                strcpy(w, saved);
            }
            return;
        }
    }
}

static void step4(char* w) {
    const char* suffixes[] = {
        "al", "ance", "ence", "er", "ic", "able", "ible",
        "ant", "ement", "ment", "ent", "ion", "ou", "ism",
        "ate", "iti", "ous", "ive", "ize"
    };
    int num = (int)(sizeof(suffixes) / sizeof(suffixes[0]));

    for (int i = 0; i < num; i++) {
        if (ends_with(w, suffixes[i])) {
            int wlen = (int)strlen(w);
            int slen = (int)strlen(suffixes[i]);
            char saved[512];
            strcpy(saved, w);
            w[wlen - slen] = '\0';

            if (strcmp(suffixes[i], "ion") == 0) {
                int tlen = (int)strlen(w);
                if (tlen > 0 && (w[tlen - 1] == 's' || w[tlen - 1] == 't')) {
                    if (measure(w) > 1) {
                        return;
                    }
                }
                strcpy(w, saved);
            } else {
                if (measure(w) > 1) {
                    return;
                }
                strcpy(w, saved);
            }
            return;
        }
    }
}

static void step5a(char* w) {
    int len = (int)strlen(w);
    if (len > 1 && w[len - 1] == 'e') {
        w[len - 1] = '\0';
        int m = measure(w);
        if (m > 1) {
            return;
        }
        if (m == 1 && !ends_cvc(w)) {
            return;
        }
        w[len - 1] = 'e';
    }
}

static void step5b(char* w) {
    int len = (int)strlen(w);
    if (len >= 2 && w[len - 1] == 'l' && w[len - 1] == w[len - 2]) {
        w[len - 1] = '\0';
        if (measure(w) <= 1) {
            w[len - 1] = 'l';
        }
    }
}

void porter_stem(char* word) {
    int len = (int)strlen(word);
    if (len <= 2 || len > 500) {
        return;
    }

    step1a(word);
    step1b(word);
    step1c(word);
    step2(word);
    step3(word);
    step4(word);
    step5a(word);
    step5b(word);
}
