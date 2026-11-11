#ifndef TOKEN_H
#define TOKEN_H

/* Estruturas compartilhadas entre pré-processador e analisador léxico. */

#define TAM_NOME_TOKEN 50
#define TAM_LEXEMA 100
#define TAM_CATEGORIA 30

/* Unidade léxica reconhecida e sua posição (1-based). */
typedef struct {
    char nome[TAM_NOME_TOKEN];
    char lexema[TAM_LEXEMA];
    int linha;
    int coluna;
} Token;

/* Entrada da Tabela de Símbolos (posição da primeira ocorrência). */
typedef struct {
    char lexema[TAM_LEXEMA];
    char categoria[TAM_CATEGORIA];
    int primeiraLinha;
    int primeiraColuna;
} Simbolo;

#endif
