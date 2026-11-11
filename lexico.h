#ifndef LEXICO_H
#define LEXICO_H

#include <stdio.h>
#include "token.h"

/*
 * Lê o arquivo de saída do pré-processamento (.pre) em 'in' e grava os tokens (.lex) em 'out'.
 * A Tabela de Símbolos e a lista de erros ficam guardadas no módulo até a
 * próxima chamada e são consultadas pelas funções abaixo.
 */
void AnaliseLexica(FILE *in, FILE *out);

/* Grava a Tabela de Símbolos em CSV (LEXEMA,CATEGORIA,LINHA,COLUNA). Chamar após AnaliseLexica. */
void GravarTabelaSimbolos(FILE *out);

/* Grava os erros léxicos no formato <tipo, lexema> linha coluna. Chamar após AnaliseLexica. */
void GravarErros(FILE *out);

/*
 * Quantidade de erros léxicos da última AnaliseLexica. É por esta consulta
 * que main() decide se a análise sintática pode ser executada.
 */
int QuantidadeErrosLexicos(void);

#endif
