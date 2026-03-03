#include "tokenizer.h"
#include <cctype>

static bool is_punctuation(char c) {
    return c == '.' || c == ',' || c == ';' || c == ':' || c == '!'
        || c == '?' || c == '"' || c == '\'' || c == '(' || c == ')'
        || c == '[' || c == ']' || c == '{' || c == '}' || c == '/'
        || c == '\\' || c == '@' || c == '#' || c == '$' || c == '%'
        || c == '^' || c == '&' || c == '*' || c == '+' || c == '='
        || c == '~' || c == '`' || c == '<' || c == '>';
}

static bool is_separator(char c) {
    return std::isspace(static_cast<unsigned char>(c)) || is_punctuation(c);
}

static bool is_pure_number(const std::string& s) {
    for (size_t i = 0; i < s.size(); i++) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) {
            return false;
        }
    }
    return true;
}

static std::string to_lower(const std::string& s) {
    std::string result = s;
    for (size_t i = 0; i < result.size(); i++) {
        result[i] = std::tolower(static_cast<unsigned char>(result[i]));
    }
    return result;
}

static void add_token_if_valid(std::vector<Token>& tokens,
                               const std::string& word, int position) {
    if (word.size() < 2) {
        return;
    }
    std::string lower = to_lower(word);
    if (is_pure_number(lower)) {
        return;
    }
    Token t;
    t.text = lower;
    t.position = position;
    tokens.push_back(t);
}

std::vector<Token> tokenize(const std::string& text) {
    std::vector<Token> tokens;
    int pos = 0;
    int i = 0;
    int len = (int)text.size();

    while (i < len) {
        while (i < len && is_separator(text[i])) {
            i++;
        }
        if (i >= len) {
            break;
        }

        std::string word;

        while (i < len && !is_separator(text[i]) && text[i] != '-') {
            word += text[i];
            i++;
        }

        if (i < len && text[i] == '-') {
            add_token_if_valid(tokens, word, pos);
            pos++;
            i++;

            while (i < len && !is_separator(text[i]) && text[i] != '-') {
                std::string part;
                while (i < len && !is_separator(text[i]) && text[i] != '-') {
                    part += text[i];
                    i++;
                }
                add_token_if_valid(tokens, part, pos);
                pos++;
                if (i < len && text[i] == '-') {
                    i++;
                }
            }
        } else {
            add_token_if_valid(tokens, word, pos);
            pos++;
        }
    }

    return tokens;
}

std::vector<std::string> tokenize_simple(const std::string& text) {
    std::vector<Token> tokens = tokenize(text);
    std::vector<std::string> result;
    for (size_t i = 0; i < tokens.size(); i++) {
        result.push_back(tokens[i].text);
    }
    return result;
}
