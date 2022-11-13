#include <stddef.h>
#include <raylib.h>

typedef struct {
    char  *str;
    size_t offset;
    size_t length;
    Color bgcolor;
    Color fgcolor;
} Token;

typedef struct Lexer Lexer;
struct Lexer {
    bool (*next)(Lexer*, Token*);
    void (*free)(Lexer*);
};

void Lexer_free(Lexer *lex);
bool Lexer_next(Lexer *lex, Token *token);