#include "query_parser.h"
#include <cstdlib>
#include <cstring>

enum QTokenType { QTOK_TERM, QTOK_AND, QTOK_OR, QTOK_NOT, QTOK_LPAREN, QTOK_RPAREN, QTOK_END };

struct QToken {
    QTokenType type;
    char* text;
};

struct TokenList {
    QToken* tokens;
    int count;
    int capacity;
    int pos;
};

static void tlist_init(TokenList* tl) {
    tl->capacity = 32;
    tl->count = 0;
    tl->pos = 0;
    tl->tokens = (QToken*)malloc(tl->capacity * sizeof(QToken));
}

static void tlist_push(TokenList* tl, QTokenType type, const char* text) {
    if (tl->count == tl->capacity) {
        tl->capacity *= 2;
        tl->tokens = (QToken*)realloc(tl->tokens, tl->capacity * sizeof(QToken));
    }
    tl->tokens[tl->count].type = type;
    if (text != NULL) {
        int len = (int)strlen(text);
        tl->tokens[tl->count].text = (char*)malloc(len + 1);
        strcpy(tl->tokens[tl->count].text, text);
    } else {
        tl->tokens[tl->count].text = NULL;
    }
    tl->count++;
}

static void tlist_free(TokenList* tl) {
    for (int i = 0; i < tl->count; i++) {
        if (tl->tokens[i].text != NULL) {
            free(tl->tokens[i].text);
        }
    }
    free(tl->tokens);
}

static bool is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static bool is_term_char(char c) {
    if (c == '\0') return false;
    if (is_ws(c)) return false;
    if (c == '(' || c == ')' || c == '!') return false;
    if (c == '&' || c == '|') return false;
    return true;
}

static void tokenize_query(const char* query, TokenList* tl) {
    int i = 0;

    while (query[i] != '\0') {
        while (is_ws(query[i])) {
            i++;
        }
        if (query[i] == '\0') {
            break;
        }

        if (query[i] == '(') {
            tlist_push(tl, QTOK_LPAREN, NULL);
            i++;
        } else if (query[i] == ')') {
            tlist_push(tl, QTOK_RPAREN, NULL);
            i++;
        } else if (query[i] == '!') {
            tlist_push(tl, QTOK_NOT, NULL);
            i++;
        } else if (query[i] == '&' && query[i + 1] == '&') {
            tlist_push(tl, QTOK_AND, NULL);
            i += 2;
        } else if (query[i] == '|' && query[i + 1] == '|') {
            tlist_push(tl, QTOK_OR, NULL);
            i += 2;
        } else {
            int start = i;
            while (is_term_char(query[i])) {
                i++;
            }
            int len = i - start;
            char* buf = (char*)malloc(len + 1);
            memcpy(buf, query + start, len);
            buf[len] = '\0';
            tlist_push(tl, QTOK_TERM, buf);
            free(buf);
        }
    }

    tlist_push(tl, QTOK_END, NULL);
}

static QToken* current(TokenList* tl) {
    return &tl->tokens[tl->pos];
}

static void advance(TokenList* tl) {
    if (tl->pos < tl->count - 1) {
        tl->pos++;
    }
}

static QueryNode* make_term_node(const char* term) {
    QueryNode* n = (QueryNode*)malloc(sizeof(QueryNode));
    n->type = NODE_TERM;
    int len = (int)strlen(term);
    n->term = (char*)malloc(len + 1);
    strcpy(n->term, term);
    n->left = NULL;
    n->right = NULL;
    return n;
}

static QueryNode* make_binary_node(NodeType type, QueryNode* left, QueryNode* right) {
    QueryNode* n = (QueryNode*)malloc(sizeof(QueryNode));
    n->type = type;
    n->term = NULL;
    n->left = left;
    n->right = right;
    return n;
}

static QueryNode* make_not_node(QueryNode* operand) {
    QueryNode* n = (QueryNode*)malloc(sizeof(QueryNode));
    n->type = NODE_NOT;
    n->term = NULL;
    n->left = operand;
    n->right = NULL;
    return n;
}

static QueryNode* parse_or_expr(TokenList* tl);

static QueryNode* parse_primary(TokenList* tl) {
    QToken* tok = current(tl);

    if (tok->type == QTOK_TERM) {
        QueryNode* n = make_term_node(tok->text);
        advance(tl);
        return n;
    }

    if (tok->type == QTOK_LPAREN) {
        advance(tl);
        QueryNode* n = parse_or_expr(tl);
        if (current(tl)->type == QTOK_RPAREN) {
            advance(tl);
        }
        return n;
    }

    return NULL;
}

static QueryNode* parse_not_expr(TokenList* tl) {
    if (current(tl)->type == QTOK_NOT) {
        advance(tl);
        QueryNode* operand = parse_not_expr(tl);
        return make_not_node(operand);
    }
    return parse_primary(tl);
}

static bool is_and_boundary(TokenList* tl) {
    QTokenType t = current(tl)->type;
    return t == QTOK_TERM || t == QTOK_NOT || t == QTOK_LPAREN;
}

static QueryNode* parse_and_expr(TokenList* tl) {
    QueryNode* left = parse_not_expr(tl);

    while (true) {
        if (current(tl)->type == QTOK_AND) {
            advance(tl);
            QueryNode* right = parse_not_expr(tl);
            left = make_binary_node(NODE_AND, left, right);
        } else if (is_and_boundary(tl)) {
            QueryNode* right = parse_not_expr(tl);
            left = make_binary_node(NODE_AND, left, right);
        } else {
            break;
        }
    }

    return left;
}

static QueryNode* parse_or_expr(TokenList* tl) {
    QueryNode* left = parse_and_expr(tl);

    while (current(tl)->type == QTOK_OR) {
        advance(tl);
        QueryNode* right = parse_and_expr(tl);
        left = make_binary_node(NODE_OR, left, right);
    }

    return left;
}

QueryNode* parse_query(const char* query) {
    TokenList tl;
    tlist_init(&tl);
    tokenize_query(query, &tl);

    QueryNode* root = parse_or_expr(&tl);

    tlist_free(&tl);
    return root;
}

void free_query(QueryNode* node) {
    if (node == NULL) {
        return;
    }
    free_query(node->left);
    free_query(node->right);
    if (node->term != NULL) {
        free(node->term);
    }
    free(node);
}
