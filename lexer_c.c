#include <ctype.h>
#include <string.h>
#include <assert.h>
#include "lexer_c.h"

static bool isoperat(char c)
{
    return c == '+' || c == '-'
        || c == '*' || c == '/'
        || c == '%' || c == '='
        || c == '!'
        || c == '<' || c == '>'
        || c == '|' || c == '&';
}

static bool iskword(const char *str, long len)
{
    static const struct {
        int  len;
        char str[8]; // Maximum length of a keyword.
    } keywords[] = {
        #define KWORD(lit) { sizeof(lit)-1, lit }
        KWORD("auto"),     KWORD("break"),   KWORD("case"), 
        KWORD("char"),     KWORD("const"),   KWORD("continue"), 
        KWORD("default"),  KWORD("do"),      KWORD("double"), 
        KWORD("else"),     KWORD("enum"),    KWORD("extern"), 
        KWORD("float"),    KWORD("for"),     KWORD("goto"),
        KWORD("if"),       KWORD("int"),     KWORD("long"), 
        KWORD("register"), KWORD("return"),  KWORD("short"), 
        KWORD("signed"),   KWORD("sizeof"),  KWORD("static"), 
        KWORD("struct"),   KWORD("switch"),  KWORD("typedef"), 
        KWORD("union"),    KWORD("unsigned"),KWORD("void"), 
        KWORD("volatile"), KWORD("while"),
        #undef KWORD
    };

    const int num_keywords = sizeof(keywords)/sizeof(keywords[0]);
    for(int i = 0; i < num_keywords; i += 1)
        if(keywords[i].len == len && !strncmp(keywords[i].str, str, len))
            return true;
    return false;
}

typedef enum {
    TK_OTHER,
    TK_NEWL,
    TK_TAB,
    TK_SPACE,
    TK_DIRECTIVE,
} SomeTokenKinds;

static bool next(Lexer *lex, Token *tok)
{
    CLexer *clex = (CLexer*) lex;
    const CLexerStyle *style = clex->style;

    size_t i = clex->cur;
    const size_t len = clex->len;
    const char  *str = clex->str;

    if(i == len)
        return false;

    SomeTokenKinds kind = TK_OTHER;
    if(i+1 < len && str[i] == '/' && str[i+1] == '/') {
        tok->fgcolor = style->fgcolor_comment;
        tok->bgcolor = style->bgcolor_comment;
        tok->offset = i;
        while(i < len && str[i] != '\n') // What about backslashes??
            i += 1;
        tok->length = i - tok->offset;
    } else if(i+1 < len && str[i] == '/' && str[i+1] == '*') {
        tok->fgcolor = style->fgcolor_comment;
        tok->bgcolor = style->bgcolor_comment;
        tok->offset = i;
        while(1) {

            while(i < len && str[i] != '*')
                i += 1;

            if(i == len)
                break;

            assert(str[i] == '*');
            i += 1;

            if(i == len)
                break;

            if(str[i] == '/') {
                i += 1;
                break;
            }
        }
        tok->length = i - tok->offset;
    } else if(str[i] == ' ') {
        kind = TK_SPACE;
        tok->fgcolor = style->fgcolor_space;
        tok->bgcolor = style->bgcolor_space;
        tok->offset = i;
        do
            i += 1;
        while(i < len && str[i] == ' ');
        tok->length = i - tok->offset;
    } else if(str[i] == '\t') {
        kind = TK_TAB;
        tok->fgcolor = style->fgcolor_tab;
        tok->bgcolor = style->bgcolor_tab;
        tok->offset = i;
        do
            i += 1;
        while(i < len && str[i] == ' ');
        tok->length = i - tok->offset;
    } else if(str[i] == '\n') {
        kind = TK_NEWL;
        tok->fgcolor = style->fgcolor_newline;
        tok->bgcolor = style->bgcolor_newline;
        tok->offset = i;
        do
            i += 1;
        while(i < len && str[i] == '\n');
        tok->length = i - tok->offset;
    } else if(str[i] == '\'' || str[i] == '\"') {
        
        char f = str[i];

        if (f == '"') {
            tok->fgcolor = style->fgcolor_literal_string;
            tok->bgcolor = style->bgcolor_literal_string;
        } else {
            tok->fgcolor = style->fgcolor_literal_charact;
            tok->bgcolor = style->bgcolor_literal_charact;
        }
        tok->offset = i;

        i += 1; // Skip the '\'' or '"'.
        
        do {
            while(i < len && str[i] != f && str[i] != '\\')
                i += 1;

            if(i == len || str[i] == f)
                break;

            if(str[i] == '\\') {
                i += 1; // Skip the '\\'.
                if(i < len)
                    i += 1; // ..and the character after it.
            }

        } while(1);

        if(i < len) {
            assert(str[i] == f);
            i += 1; // Skip the final '\'' or '"'.
        }
        tok->length = i - tok->offset;

    } else if(isdigit(str[i])) {

        tok->offset = i;

        // We allow an 'x' if it's after a '0'.
        if(i+2 < len && str[i] == '0' && str[i+1] == 'x' && isdigit(str[i+2]))
            i += 2; // Skip the '0x'.

        while(i < len && isdigit(str[i]))
            i += 1;
        
        // If the next character is a dot followed
        // by a digit, then we continue to scan.    
        if(i+1 < len && str[i] == '.' && isdigit(str[i+1])) {
            i += 1; // Skip the '.'.
            while(i < len && isdigit(str[i]))
                i += 1;
            tok->fgcolor = style->fgcolor_literal_floating;
            tok->bgcolor = style->bgcolor_literal_floating;
        } else {
            tok->fgcolor = style->fgcolor_literal_integer;
            tok->bgcolor = style->bgcolor_literal_integer;
        }
        tok->length = i - tok->offset;
    
    } else if(isalpha(str[i]) || str[i] == '_') {

        tok->offset = i;
        do
            i += 1;
        while(isalpha(str[i]) || 
              isdigit(str[i]) || str[i] == '_');
        tok->length = i - tok->offset;

        /* It may either be an identifier or a
         * language keyword.
         */
        
        if(iskword(str + tok->offset, tok->length)) {
            tok->fgcolor = style->fgcolor_keyword;
            tok->bgcolor = style->bgcolor_keyword;
        } else {

            /* If the identifier is followed by a '(' and
             * it's in the global scope, then it's for a
             * function definiton. If it's not in the global
             * scope then it's a function call.
             * Between the identifier and the '(' there may
             * be some whitespace. An exception is made if
             * before the identifier comes a preprocessor
             * directive, in which case the '(' must come
             * right after the identifier.
             */

            bool followed_by_parenthesis = false;
            bool yes_and_immediately = false;
            {
                size_t k = i;
                while(k < len && (str[k] == ' ' || str[k] == '\t'))
                    k += 1;

                if(k < len && str[k] == '(') {
                    followed_by_parenthesis = true;
                    if(k == i)
                        yes_and_immediately = true;
                }
            }

            Color fgcolor, bgcolor;
            if(followed_by_parenthesis) {
                if(clex->curly_bracket_depth == 0) {
                    if(clex->prev_nonspace_was_directive) {
                        if(yes_and_immediately) {
                            fgcolor = style->fgcolor_func_decl_name;
                            bgcolor = style->bgcolor_func_decl_name;
                        } else {
                            fgcolor = style->fgcolor_identifier;
                            bgcolor = style->bgcolor_identifier;
                        }
                    } else {
                        fgcolor = style->fgcolor_func_decl_name;
                        bgcolor = style->bgcolor_func_decl_name;
                    }
                } else {
                    fgcolor = style->fgcolor_func_call_name;
                    bgcolor = style->bgcolor_func_call_name;
                }
            } else {
                fgcolor = style->fgcolor_identifier;
                bgcolor = style->bgcolor_identifier;
            }

            tok->fgcolor = fgcolor;
            tok->bgcolor = bgcolor;
        }
    
    } else if(str[i] == '#' && clex->only_spaces_since_line_start) {

        // The first non-whitespace tok of the line
        // is a '#'. If it's followed by an alphabetical
        // character, then it's a directive. (There may
        // be whitespace between the '#' and the identifier)

        size_t j = i; // Use a secondary cursor to explore
                      // what's after the '#'.

        j += 1; // Skip the '#'.

        // Skip spaces after the '#', if there are any.
        while(j < len && (str[j] == ' ' || str[j] == '\t'))
            j += 1;

        if(isalpha(str[j])) {

            // It's a preprocessor directive!
            kind = TK_DIRECTIVE;
            tok->fgcolor = style->fgcolor_directive;
            tok->bgcolor = style->bgcolor_directive;
            tok->offset = i;
            
            while(j < len && isalpha(str[j]))
                j += 1;

            tok->length = j - tok->offset;
            
            i = j;

        } else {
            // Wasn't a directive.. Just tokize the '#'.
            tok->fgcolor = style->fgcolor_other;
            tok->bgcolor = style->bgcolor_other;
            tok->offset = i;
            tok->length = 1;
            i += 1;
        }

    } else if(str[i] == '<' && clex->prev_nonspace_was_directive) {

        tok->fgcolor = style->fgcolor_literal_string;
        tok->bgcolor = style->bgcolor_literal_string;
        tok->offset = i;
        while(i < len && str[i] != '>')
            i += 1;
        if(i < len)
            i += 1; // Skip the '>'.
        tok->length = i - tok->offset;

    } else if(isoperat(str[i])) {

        tok->fgcolor = style->fgcolor_operator;
        tok->bgcolor = style->bgcolor_operator;
        tok->offset = i;
        while(i < len && isoperat(str[i]))
            i += 1;
        tok->length = i - tok->offset;
    } else {

        switch(str[i]) {
            case '{': clex->curly_bracket_depth += 1; break;
            case '}': clex->curly_bracket_depth -= 1; break;
        }

        tok->fgcolor = style->fgcolor_other;
        tok->bgcolor = style->bgcolor_other;
        tok->offset = i;
        tok->length = 1;
        i += 1;
    }

    if(kind == TK_NEWL)
        clex->only_spaces_since_line_start = true;
    else
        if(kind != TK_TAB && kind != TK_SPACE)
            clex->only_spaces_since_line_start = false;

    if(kind == TK_DIRECTIVE)
        clex->prev_nonspace_was_directive = true;
    else
        if(kind != TK_TAB && kind != TK_SPACE)
            clex->prev_nonspace_was_directive = false;

    clex->cur = i;
    return true;
}

// static, const EC5F66

static const CLexerStyle default_style = {
    .fgcolor_other   = {255, 255, 255, 255},
    .bgcolor_other   = {0,   0,   0,   0},
    .fgcolor_comment = {166, 172, 185, 255},
    .bgcolor_comment = {0,   0,   0,   0},
    .fgcolor_space   = {0,   0,   0,   0},
    .bgcolor_space   = {0,   0,   0,   0},
    .fgcolor_tab     = {0,   0,   0,   0},
    .bgcolor_tab     = {0,   0,   0,   0},
    .fgcolor_newline = {0,   0,   0,   0},
    .bgcolor_newline = {0,   0,   0,   0},
    .fgcolor_literal_string   = {153, 199, 148, 255},
    .bgcolor_literal_string   = {0,   0,   0,   0},
    .fgcolor_literal_charact  = {153, 199, 148, 255},
    .bgcolor_literal_charact  = {0,   0,   0,   0},
    .fgcolor_literal_integer  = {249, 174, 88,  255},
    .bgcolor_literal_integer  = {0,   0,   0,   0},
    .fgcolor_literal_floating = {249, 174, 88,  255},
    .bgcolor_literal_floating = {0,   0,   0,   0},
    .fgcolor_keyword        = {198, 149, 198, 255},
    .bgcolor_keyword        = {0,   0,   0,   0},
    .fgcolor_func_decl_name = {95,  180, 180, 255},
    .bgcolor_func_decl_name = {0,   0,   0,   0},
    .fgcolor_func_call_name = {102, 153, 204, 255},
    .bgcolor_func_call_name = {0,   0,   0,   0},
    .fgcolor_identifier     = {216, 222, 233, 255},
    .bgcolor_identifier     = {0,   0,   0,   0},
    .fgcolor_operator       = {249, 123, 88,  255},
    .bgcolor_operator       = {0,   0,   0,   0  },
    .fgcolor_directive      = {198, 149, 198, 255},
    .bgcolor_directive      = {0,   0,   0,   0},
};

CLexer CLexer_init(const CLexerStyle *style,
                   const char *str, size_t len)
{
    if (style == NULL)
        style = &default_style;

    CLexer clex;
    clex.base.free = NULL;
    clex.base.next = next; 
    clex.style = style;
    clex.str = str;
    clex.len = len;
    clex.cur = 0;
    clex.curly_bracket_depth = 0;
    clex.only_spaces_since_line_start = true;
    clex.prev_nonspace_was_directive  = false;
    return clex;
}
