#ifndef QUERY_PARSER_H
#define QUERY_PARSER_H

enum NodeType { NODE_TERM, NODE_AND, NODE_OR, NODE_NOT };

struct QueryNode {
    NodeType type;
    char* term;
    QueryNode* left;
    QueryNode* right;
};

QueryNode* parse_query(const char* query);
void free_query(QueryNode* node);

#endif
