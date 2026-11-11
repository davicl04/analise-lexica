#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <string.h>

/*
 * Utilitários compartilhados pelos módulos. São 'static inline' para que o
 * comando de compilação continue listando apenas os arquivos .c originais.
 */

/* strncpy que sempre termina a string. */
static inline void copiarTexto(char *destino, const char *origem, size_t tamDestino) {
    strncpy(destino, origem, tamDestino - 1);
    destino[tamDestino - 1] = '\0';
}

/* Byte de continuação de um caractere UTF-8 (10xxxxxx). Falso para EOF. */
static inline int ehContinuacaoUtf8(int c) {
    return c >= 0x80 && c <= 0xBF;
}

#endif
