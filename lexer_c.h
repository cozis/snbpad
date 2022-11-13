#include "lexer.h"

typedef struct {
    Color fgcolor_other;
    Color bgcolor_other;
    Color fgcolor_comment;
    Color bgcolor_comment;
    Color fgcolor_space;
    Color bgcolor_space;
    Color fgcolor_tab;
    Color bgcolor_tab;
    Color fgcolor_newline;
    Color bgcolor_newline;
    Color fgcolor_literal_string;
    Color bgcolor_literal_string;
    Color fgcolor_literal_charact;
    Color bgcolor_literal_charact;
    Color fgcolor_literal_integer;
    Color bgcolor_literal_integer;
    Color fgcolor_literal_floating;
    Color bgcolor_literal_floating;
    Color fgcolor_keyword;
    Color bgcolor_keyword;
    Color fgcolor_func_decl_name;
    Color bgcolor_func_decl_name;
    Color fgcolor_func_call_name;
    Color bgcolor_func_call_name;
    Color fgcolor_identifier;
    Color bgcolor_identifier;
    Color fgcolor_operator;
    Color bgcolor_operator;
    Color fgcolor_directive;
    Color bgcolor_directive;
} CLexerStyle;

typedef struct {
    Lexer base;
    const char *str;
    size_t len, cur;
    const CLexerStyle *style;
    int  curly_bracket_depth;
    bool prev_nonspace_was_directive;
    bool only_spaces_since_line_start;
} CLexer;

CLexer CLexer_init(const CLexerStyle *style,
                   const char *src, size_t len);
