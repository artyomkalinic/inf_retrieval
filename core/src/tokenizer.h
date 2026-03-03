#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <vector>
#include <string>

struct Token {
    std::string text;
    int position;
};

std::vector<Token> tokenize(const std::string& text);
std::vector<std::string> tokenize_simple(const std::string& text);

#endif
