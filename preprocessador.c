#include "preprocessador.h"

#define TAM_MAX_LINHA 4096

/* Lê uma linha aceitando \n, \r\n ou \r. Excedente além do buffer é descartado. */
static int lerLinha(FILE *in, char *buffer, int tamMax) {
    int c;
    int i = 0;
    int leuAlgumCaractere = 0;

    while ((c = fgetc(in)) != EOF) {
        leuAlgumCaractere = 1;

        if (c == '\r') {
            int proximo = fgetc(in);
            if (proximo != '\n' && proximo != EOF) {
                ungetc(proximo, in);
            }
            break;
        }
        if (c == '\n') {
            break;
        }
        if (i < tamMax - 1) {
            buffer[i++] = (char) c;
        }
    }

    buffer[i] = '\0';
    return leuAlgumCaractere;
}

/* Remove comentários e colapsa espaços/tabs fora de strings. */
static void processarLinha(const char *linha, char *saida, int tamMax) {
    int i = 0;
    int j = 0;
    int dentroString = 0;
    int ultimoFoiEspaco = 1; /* descarta espaços à esquerda */

    while (linha[i] != '\0' && j < tamMax - 1) {
        char c = linha[i];

        if (dentroString) {
            saida[j++] = c;

            /* copia o caractere escapado para não fechar a string em \" */
            if (c == '\\' && linha[i + 1] != '\0' && j < tamMax - 1) {
                i++;
                saida[j++] = linha[i];
                i++;
                continue;
            }

            if (c == '"') {
                dentroString = 0;
            }
            i++;
            continue;
        }

        if (c == '#') {
            break;
        }

        if (c == '"') {
            dentroString = 1;
            saida[j++] = c;
            ultimoFoiEspaco = 0;
            i++;
            continue;
        }

        if (c == ' ' || c == '\t') {
            if (!ultimoFoiEspaco) {
                saida[j++] = ' ';
                ultimoFoiEspaco = 1;
            }
            i++;
            continue;
        }

        saida[j++] = c;
        ultimoFoiEspaco = 0;
        i++;
    }

    /* remove espaço final */
    if (j > 0 && saida[j - 1] == ' ') {
        j--;
    }

    saida[j] = '\0';
}

void PreProcessamento(FILE *in, FILE *out) {
    char linha[TAM_MAX_LINHA];
    char saida[TAM_MAX_LINHA];

    while (lerLinha(in, linha, TAM_MAX_LINHA)) {
        processarLinha(linha, saida, TAM_MAX_LINHA);

        if (saida[0] != '\0') {
            fprintf(out, "%s\n", saida);
        }
    }
}
