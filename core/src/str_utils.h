#ifndef STR_UTILS_H
#define STR_UTILS_H

#include <cstdlib>

inline int str_len(const char* s) {
    int len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

inline int str_cmp(const char* a, const char* b) {
    int i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return (unsigned char)a[i] - (unsigned char)b[i];
        }
        i++;
    }
    return (unsigned char)a[i] - (unsigned char)b[i];
}

inline bool str_eq(const char* a, const char* b) {
    return str_cmp(a, b) == 0;
}

inline char* str_dup(const char* s) {
    int len = str_len(s);
    char* copy = (char*)malloc(len + 1);
    for (int i = 0; i <= len; i++) {
        copy[i] = s[i];
    }
    return copy;
}

inline void str_to_lower(char* s) {
    for (int i = 0; s[i] != '\0'; i++) {
        if (s[i] >= 'A' && s[i] <= 'Z') {
            s[i] = s[i] + ('a' - 'A');
        }
    }
}

inline void str_copy(char* dst, const char* src, int max_len) {
    int i = 0;
    while (i < max_len - 1 && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

inline bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

inline bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

inline bool is_alnum(char c) {
    return is_alpha(c) || is_digit(c);
}

inline bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

inline int str_to_int(const char* s) {
    int result = 0;
    int sign = 1;
    int i = 0;

    while (is_space(s[i])) {
        i++;
    }

    if (s[i] == '-') {
        sign = -1;
        i++;
    } else if (s[i] == '+') {
        i++;
    }

    while (is_digit(s[i])) {
        result = result * 10 + (s[i] - '0');
        i++;
    }

    return result * sign;
}

#endif
