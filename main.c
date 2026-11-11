#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "preprocessador.h"
#include "lexico.h"
#include "sintatico.h"

#ifdef _WIN32
#include <windows.h>
#endif

/*
 * Uso: main.exe <arquivo_entrada.asm> <prefixo_saida>
 *
 * Gera <prefixo>.pre, .lex, .ts, .lex.err, .sin.err e .dot, executando, nesta
 * ordem: pré-processamento, análise léxica e, se não houver erros léxicos,
 * análise sintática.
 */

/* Códigos de encerramento do programa. */
#define SAIDA_VALIDO     0  /* execução concluída e programa sintaticamente válido */
#define SAIDA_REJEITADO  1  /* erros léxicos ou sintáticos */
#define SAIDA_FALHA      2  /* falha operacional ou uso incorreto */

#define TAM_MAX_NOME 512

enum { ARQ_PRE, ARQ_LEX, ARQ_TS, ARQ_LEX_ERR, ARQ_SIN_ERR, ARQ_DOT, QTD_SAIDAS };

static const char *const SUFIXOS[QTD_SAIDAS] = {
    ".pre", ".lex", ".ts", ".lex.err", ".sin.err", ".dot"
};

/*
 * Cria (ou trunca) todos os arquivos de saída de uma vez, para que nenhum
 * resultado de execução anterior sobreviva. São abertos para leitura e
 * escrita porque o .pre é relido pela análise léxica e o .lex, pela sintática.
 * Em caso de falha, fecha os que já abriu e retorna 0.
 */
static int abrirSaidas(const char *prefixo, FILE *saidas[QTD_SAIDAS]) {
    char nome[TAM_MAX_NOME];
    int i;

    for (i = 0; i < QTD_SAIDAS; i++) {
        if (strlen(prefixo) + strlen(SUFIXOS[i]) >= sizeof(nome)) {
            fprintf(stderr, "Erro: prefixo de saída longo demais.\n");
            break;
        }
        strcpy(nome, prefixo);
        strcat(nome, SUFIXOS[i]);

        saidas[i] = fopen(nome, "w+b");
        if (saidas[i] == NULL) {
            fprintf(stderr, "Erro: não foi possível criar o arquivo '%s'.\n", nome);
            break;
        }
    }

    if (i < QTD_SAIDAS) {
        while (--i >= 0) {
            fclose(saidas[i]);
        }
        return 0;
    }
    return 1;
}

/* Executa as três etapas e devolve o código de encerramento. */
static int executarEtapas(FILE *entrada, FILE *saidas[QTD_SAIDAS]) {
    /* Etapa 1: pré-processamento */
    PreProcessamento(entrada, saidas[ARQ_PRE]);

    if (ferror(entrada)) {
        fprintf(stderr, "Erro: falha de leitura no arquivo de entrada.\n");
        return SAIDA_FALHA;
    }

    /*
     * Etapa 2: análise léxica, lendo o .pre desde o início. Linha e coluna
     * dos tokens e dos diagnósticos referem-se, portanto, ao .pre.
     */
    rewind(saidas[ARQ_PRE]);
    AnaliseLexica(saidas[ARQ_PRE], saidas[ARQ_LEX]);
    GravarTabelaSimbolos(saidas[ARQ_TS]);
    GravarErros(saidas[ARQ_LEX_ERR]);

    if (QuantidadeErrosLexicos() > 0) {
        fputs("Análise sintática não executada: existem erros léxicos.\n", saidas[ARQ_SIN_ERR]);
        puts("ANÁLISE SINTÁTICA NÃO EXECUTADA: EXISTEM ERROS LÉXICOS.");
        return SAIDA_REJEITADO; /* o .dot permanece vazio */
    }

    /* Etapa 3: análise sintática, relendo o .lex desde o início */
    rewind(saidas[ARQ_LEX]);
    switch (AnaliseSintatica(saidas[ARQ_LEX], saidas[ARQ_DOT], saidas[ARQ_SIN_ERR])) {
        case SINTATICO_VALIDO:
            puts("PROGRAMA SINTATICAMENTE VÁLIDO");
            return SAIDA_VALIDO;
        case SINTATICO_INVALIDO:
            puts("PROGRAMA SINTATICAMENTE INVÁLIDO");
            return SAIDA_REJEITADO;
        default:
            fprintf(stderr, "Erro: falha operacional durante a análise sintática.\n");
            return SAIDA_FALHA;
    }
}

int main(int argc, char *argv[]) {
    FILE *entrada;
    FILE *saidas[QTD_SAIDAS];
    int codigo;
    int i;

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8); /* as mensagens têm acentos em UTF-8 */
#endif

    if (argc != 3) {
        fprintf(stderr, "Uso: %s <arquivo_entrada.asm> <prefixo_saida>\n", argv[0]);
        return SAIDA_FALHA;
    }

    entrada = fopen(argv[1], "rb");
    if (entrada == NULL) {
        fprintf(stderr, "Erro: não foi possível abrir o arquivo de entrada '%s'.\n", argv[1]);
        return SAIDA_FALHA;
    }
    if (!abrirSaidas(argv[2], saidas)) {
        fclose(entrada);
        return SAIDA_FALHA;
    }

    codigo = executarEtapas(entrada, saidas);

    fclose(entrada);
    for (i = 0; i < QTD_SAIDAS; i++) {
        int falhou = ferror(saidas[i]);
        if (fclose(saidas[i]) != 0 || falhou) {
            fprintf(stderr, "Erro: falha de gravação em '%s%s'.\n", argv[2], SUFIXOS[i]);
            codigo = SAIDA_FALHA;
        }
    }

    return codigo;
}
