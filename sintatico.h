#ifndef SINTATICO_H
#define SINTATICO_H

#include <stdio.h>

/* Valores de retorno de AnaliseSintatica. */
#define SINTATICO_VALIDO     0   /* programa sintaticamente válido */
#define SINTATICO_INVALIDO   1   /* programa sintaticamente inválido */
#define SINTATICO_FALHA    (-1)  /* falha operacional (leitura, gravação, memória, .lex malformado) */

/*
 * Análise descendente recursiva do subconjunto de Assembly MIPS.
 *
 *   tokens: arquivo .lex, aberto para leitura e posicionado no início;
 *   arvore: arquivo .dot, aberto para escrita. Recebe a árvore sintática
 *           concreta somente se o programa for válido; senão fica vazio;
 *   erros:  arquivo .sin.err, aberto para escrita. Recebe os diagnósticos
 *           ou a mensagem de ausência de erros.
 *
 * A abertura e o fechamento dos arquivos são responsabilidade de quem chama.
 */
int AnaliseSintatica(FILE *tokens, FILE *arvore, FILE *erros);

#endif
