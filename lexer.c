#include "lexer.h"

void Lexer_free(Lexer *lex)
{
    if (lex->free != NULL)
        lex->free(lex);
}

bool Lexer_next(Lexer *lex, Token *token)
{
    return lex->next(lex, token);
}