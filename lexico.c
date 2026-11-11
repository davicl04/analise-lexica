#include "lexico.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Memória
 * ============================================================ */

#define CODIGO_FALHA_OPERACIONAL 2

/* realloc que encerra o programa com mensagem (código de falha operacional) se faltar memória. */
static void *realocarOuAbortar(void *ptr, size_t tamanho) {
    void *novo = realloc(ptr, tamanho);
    if (novo == NULL) {
        fprintf(stderr, "Erro: memoria insuficiente na analise lexica.\n");
        exit(CODIGO_FALHA_OPERACIONAL);
    }
    return novo;
}

/* Garante espaço para mais um item em um vetor dinâmico; devolve o vetor (possivelmente realocado). */
static void *reservarItem(void *itens, int quantidade, int *capacidade, size_t tamItem) {
    if (quantidade < *capacidade) {
        return itens;
    }
    *capacidade = (*capacidade == 0) ? 32 : *capacidade * 2;
    return realocarOuAbortar(itens, tamItem * (size_t) *capacidade);
}

/* ============================================================
 * Tabela de Símbolos e lista de erros
 * ============================================================ */

/* Categorias gravadas na Tabela de Símbolos. */
#define CAT_DIRETIVA      "diretiva"
#define CAT_INSTRUCAO     "instrução"
#define CAT_REGISTRADOR   "registrador"
#define CAT_IDENTIFICADOR "identificador/rótulo"

/* Uma entrada por lexema, com a posição da primeira ocorrência. */
typedef struct {
    Simbolo *itens;
    int quantidade;
    int capacidade;
} TabelaSimbolos;

/* Erros na ordem em que foram encontrados (aceita repetidos). Token.nome = tipo do erro. */
typedef struct {
    Token *itens;
    int quantidade;
    int capacidade;
} ListaErros;

/* Tudo o que a análise léxica produz além do fluxo de tokens. */
typedef struct {
    TabelaSimbolos simbolos;
    ListaErros erros;
} ResultadoLexico;

/* Resultado da última AnaliseLexica: único estado do módulo, lido pelas funções Gravar e de consulta. */
static ResultadoLexico resultadoAtual = {{NULL, 0, 0}, {NULL, 0, 0}};

static Simbolo *buscarSimbolo(TabelaSimbolos *ts, const char *lexema) {
    int i;
    for (i = 0; i < ts->quantidade; i++) {
        if (strcmp(ts->itens[i].lexema, lexema) == 0) {
            return &ts->itens[i];
        }
    }
    return NULL;
}

/*
 * Insere o lexema se não existir. Se já existe com posição (0,0)
 * (palavra reservada pré-carregada), registra a primeira ocorrência real.
 */
static void registrarSimbolo(TabelaSimbolos *ts, const char *lexema,
                             const char *categoria, int linha, int coluna) {
    Simbolo *existente = buscarSimbolo(ts, lexema);
    Simbolo *novo;

    if (existente != NULL) {
        if (existente->primeiraLinha == 0 && existente->primeiraColuna == 0) {
            existente->primeiraLinha = linha;
            existente->primeiraColuna = coluna;
        }
        return;
    }

    ts->itens = (Simbolo *) reservarItem(ts->itens, ts->quantidade, &ts->capacidade, sizeof(Simbolo));
    novo = &ts->itens[ts->quantidade++];
    copiarTexto(novo->lexema, lexema, TAM_LEXEMA);
    copiarTexto(novo->categoria, categoria, TAM_CATEGORIA);
    novo->primeiraLinha = linha;
    novo->primeiraColuna = coluna;
}

static void registrarErro(ListaErros *le, const char *tipo, const char *lexema, int linha, int coluna) {
    Token *erro;

    le->itens = (Token *) reservarItem(le->itens, le->quantidade, &le->capacidade, sizeof(Token));
    erro = &le->itens[le->quantidade++];
    copiarTexto(erro->nome, tipo, TAM_NOME_TOKEN);
    copiarTexto(erro->lexema, lexema, TAM_LEXEMA);
    erro->linha = linha;
    erro->coluna = coluna;
}

/* ============================================================
 * Palavras reservadas
 * ============================================================ */

typedef struct {
    const char *lexema;
    const char *tokenNome;
} PalavraReservada;

static const PalavraReservada DIRETIVAS[] = {
    {".data",   "DIR_DATA"},
    {".text",   "DIR_TEXT"},
    {".word",   "DIR_WORD"},
    {".half",   "DIR_HALF"},
    {".byte",   "DIR_BYTE"},
    {".space",  "DIR_SPACE"},
    {".ascii",  "DIR_ASCII"},
    {".asciiz", "DIR_ASCIIZ"},
    {".globl",  "DIR_GLOBL"},
    {".align",  "DIR_ALIGN"}
};
#define QTD_DIRETIVAS (sizeof(DIRETIVAS) / sizeof(DIRETIVAS[0]))

static const PalavraReservada INSTRUCOES[] = {
    /* Transferência de dados */
    {"li", "INS_LI"}, {"la", "INS_LA"}, {"move", "INS_MOVE"},
    {"lw", "INS_LW"}, {"sw", "INS_SW"}, {"lb", "INS_LB"},
    {"sb", "INS_SB"}, {"lh", "INS_LH"}, {"sh", "INS_SH"},
    /* Aritméticas */
    {"add", "INS_ADD"}, {"addi", "INS_ADDI"}, {"addu", "INS_ADDU"},
    {"sub", "INS_SUB"}, {"subu", "INS_SUBU"}, {"mul", "INS_MUL"},
    {"div", "INS_DIV"}, {"mflo", "INS_MFLO"}, {"mfhi", "INS_MFHI"},
    /* Lógicas, deslocamentos e comparação */
    {"and", "INS_AND"}, {"or", "INS_OR"}, {"xor", "INS_XOR"},
    {"nor", "INS_NOR"}, {"sll", "INS_SLL"}, {"srl", "INS_SRL"},
    {"slt", "INS_SLT"},
    /* Desvios e controle */
    {"beq", "INS_BEQ"}, {"bne", "INS_BNE"}, {"blt", "INS_BLT"},
    {"ble", "INS_BLE"}, {"bgt", "INS_BGT"}, {"bge", "INS_BGE"},
    {"j", "INS_J"}, {"jal", "INS_JAL"}, {"jr", "INS_JR"},
    {"syscall", "INS_SYSCALL"}, {"nop", "INS_NOP"}
};
#define QTD_INSTRUCOES (sizeof(INSTRUCOES) / sizeof(INSTRUCOES[0]))

static const char *const REGISTRADORES_NOMEADOS[] = {
    "$zero",
    "$v0", "$v1",
    "$a0", "$a1", "$a2", "$a3",
    "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7", "$t8", "$t9",
    "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
    "$k0", "$k1",
    "$gp", "$sp", "$fp", "$ra"
};
#define QTD_REGISTRADORES_NOMEADOS (sizeof(REGISTRADORES_NOMEADOS) / sizeof(REGISTRADORES_NOMEADOS[0]))

static int ehDigito(int c) {
    return c >= '0' && c <= '9';
}

/* Retorna o nome do token de 'lexema' na lista, ou NULL. */
static const char *buscarTokenReservado(const PalavraReservada *lista, size_t tamanho, const char *lexema) {
    size_t i;
    for (i = 0; i < tamanho; i++) {
        if (strcmp(lista[i].lexema, lexema) == 0) {
            return lista[i].tokenNome;
        }
    }
    return NULL;
}

/* 'lexema' (minúsculo, com '$') é um nome da lista ou $0..$31 sem zeros à esquerda? */
static int ehRegistradorValido(const char *lexema) {
    const char *numero = lexema + 1;
    size_t i;

    for (i = 0; i < QTD_REGISTRADORES_NOMEADOS; i++) {
        if (strcmp(REGISTRADORES_NOMEADOS[i], lexema) == 0) {
            return 1;
        }
    }

    if (!ehDigito(numero[0])) {
        return 0;
    }
    if (numero[1] == '\0') {
        return 1;                                   /* $0..$9 */
    }
    if (numero[0] == '0' || !ehDigito(numero[1]) || numero[2] != '\0') {
        return 0;
    }
    return (numero[0] - '0') * 10 + (numero[1] - '0') <= 31;
}

/* Pré-carrega diretivas, instruções e registradores ($nome e $0..$31) com posição (0,0). */
static void popularPalavrasReservadas(TabelaSimbolos *ts) {
    size_t i;
    char bufferRegistradorNumerico[16];
    int numero;

    for (i = 0; i < QTD_DIRETIVAS; i++) {
        registrarSimbolo(ts, DIRETIVAS[i].lexema, CAT_DIRETIVA, 0, 0);
    }
    for (i = 0; i < QTD_INSTRUCOES; i++) {
        registrarSimbolo(ts, INSTRUCOES[i].lexema, CAT_INSTRUCAO, 0, 0);
    }
    for (i = 0; i < QTD_REGISTRADORES_NOMEADOS; i++) {
        registrarSimbolo(ts, REGISTRADORES_NOMEADOS[i], CAT_REGISTRADOR, 0, 0);
    }
    for (numero = 0; numero <= 31; numero++) {
        snprintf(bufferRegistradorNumerico, sizeof(bufferRegistradorNumerico), "$%d", numero);
        registrarSimbolo(ts, bufferRegistradorNumerico, CAT_REGISTRADOR, 0, 0);
    }
}

/* ============================================================
 * Leitor: fonte em memória com lookahead arbitrário
 * ============================================================ */

typedef struct {
    char *fonte;
    size_t tamanho;
    size_t pos;
    int linha;   /* posição (1-based) do próximo caractere */
    int coluna;
    FILE *saida;
    ResultadoLexico *resultado;
} Analisador;

static char *lerArquivoInteiro(FILE *in, size_t *tamanho) {
    char *buffer = NULL;
    size_t capacidade = 0;
    size_t lidos = 0;
    size_t n;

    do {
        if (lidos == capacidade) {
            capacidade = (capacidade == 0) ? 4096 : capacidade * 2;
            buffer = (char *) realocarOuAbortar(buffer, capacidade);
        }
        n = fread(buffer + lidos, 1, capacidade - lidos, in);
        lidos += n;
    } while (n > 0);

    *tamanho = lidos;
    return buffer;
}

/*
 * Caractere 'deslocamento' posições à frente, sem consumir (EOF no fim).
 * '\r' é visto como '\n': o fonte original pode usar \n, \r\n ou \r.
 */
static int espiarEm(const Analisador *a, size_t deslocamento) {
    size_t i = a->pos + deslocamento;
    int c;

    if (i >= a->tamanho) {
        return EOF;
    }
    c = (unsigned char) a->fonte[i];
    return (c == '\r') ? '\n' : c;
}

static int espiar(const Analisador *a) {
    return espiarEm(a, 0);
}

/*
 * Consome o próximo caractere, atualizando linha/coluna. "\r\n" conta como
 * uma única quebra de linha, e a coluna conta caracteres, não bytes: os
 * bytes de continuação UTF-8 não a avançam.
 */
static int avancar(Analisador *a) {
    int c = espiar(a);

    if (c == EOF) {
        return EOF;
    }
    if (a->fonte[a->pos] == '\r' && a->pos + 1 < a->tamanho && a->fonte[a->pos + 1] == '\n') {
        a->pos++;
    }
    a->pos++;
    if (c == '\n') {
        a->linha++;
        a->coluna = 1;
    } else if (!ehContinuacaoUtf8(c)) {
        a->coluna++;
    }
    return c;
}

/* ============================================================
 * Classes de caracteres e montagem de lexemas
 * ============================================================ */

static int ehLetra(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int ehDigitoHex(int c) {
    return ehDigito(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int ehCaractereDePalavra(int c) {
    return ehLetra(c) || ehDigito(c) || c == '_';
}

/* Caracteres que podem encerrar um token; qualquer outro colado a ele o torna malformado. */
static int ehDelimitador(int c) {
    return c == EOF || c == ' ' || c == '\t' || c == '\n'
        || c == ',' || c == ':' || c == '(' || c == ')'
        || c == '#' || c == '"';
}

static void paraMinusculas(const char *origem, char *destino, int tamDestino) {
    int i;
    for (i = 0; origem[i] != '\0' && i < tamDestino - 1; i++) {
        char c = origem[i];
        if (c >= 'A' && c <= 'Z') {
            c = (char) (c - 'A' + 'a');
        }
        destino[i] = c;
    }
    destino[i] = '\0';
}

/* Lexema sempre terminado em '\0'; o excedente além de TAM_LEXEMA é descartado. */
typedef struct {
    char texto[TAM_LEXEMA];
    int tamanho;
} Lexema;

static void iniciarLexema(Lexema *lx) {
    lx->tamanho = 0;
    lx->texto[0] = '\0';
}

static void acrescentar(Lexema *lx, int c) {
    if (lx->tamanho < TAM_LEXEMA - 1) {
        lx->texto[lx->tamanho++] = (char) c;
        lx->texto[lx->tamanho] = '\0';
    }
}

/* Consome um caractere e o acrescenta ao lexema. */
static int consumirPara(Analisador *a, Lexema *lx) {
    int c = avancar(a);
    acrescentar(lx, c);
    return c;
}

static void consumirPalavra(Analisador *a, Lexema *lx) {
    while (ehCaractereDePalavra(espiar(a))) {
        consumirPara(a, lx);
    }
}

static void consumirAteDelimitador(Analisador *a, Lexema *lx) {
    while (!ehDelimitador(espiar(a))) {
        consumirPara(a, lx);
    }
}

/* ============================================================
 * Saída
 * ============================================================ */

static void emitirToken(Analisador *a, const char *nomeToken, const char *lexema, int linha, int coluna) {
    fprintf(a->saida, "<%s, %s> %d %d\n", nomeToken, lexema, linha, coluna);
}

/*
 * Emite o token com o lexema original e registra 'lexemaTS' na TS. Para
 * palavras reservadas, 'lexemaTS' é o lexema normalizado (minúsculo).
 */
static void emitirERegistrar(Analisador *a, const char *nomeToken, const char *lexema,
                             const char *lexemaTS, const char *categoria,
                             int linha, int coluna) {
    emitirToken(a, nomeToken, lexema, linha, coluna);
    registrarSimbolo(&a->resultado->simbolos, lexemaTS, categoria, linha, coluna);
}

static void erro(Analisador *a, const char *tipo, const char *lexema, int linha, int coluna) {
    registrarErro(&a->resultado->erros, tipo, lexema, linha, coluna);
}

/* ============================================================
 * AFD: estados de reconhecimento
 * Cada função é chamada com o primeiro caractere do token ainda não consumido.
 * ============================================================ */

static void consumirComentario(Analisador *a) {
    while (espiar(a) != '\n' && espiar(a) != EOF) {
        avancar(a);
    }
}

/*
 * (letra|_)(letra|dígito|_)*: emite INS_* se for instrução (sem
 * diferenciar maiúsculas), senão ID (sensível a maiúsculas).
 * Se o nome estiver colado a um não-delimitador (ex.: "soma-total"),
 * consome até o delimitador e registra ERRO_IDENTIFICADOR_MALFORMADO.
 * "add:" gera INS_ADD + SMB_COL sem erro (validação é sintática).
 */
static void reconhecerIdentificadorOuInstrucao(Analisador *a) {
    int linha = a->linha, coluna = a->coluna;
    Lexema lx;
    char minusculo[TAM_LEXEMA];
    const char *token;

    iniciarLexema(&lx);
    consumirPalavra(a, &lx);

    if (!ehDelimitador(espiar(a))) {
        consumirAteDelimitador(a, &lx);
        erro(a, "ERRO_IDENTIFICADOR_MALFORMADO", lx.texto, linha, coluna);
        return;
    }

    paraMinusculas(lx.texto, minusculo, TAM_LEXEMA);
    token = buscarTokenReservado(INSTRUCOES, QTD_INSTRUCOES, minusculo);

    if (token != NULL) {
        emitirERegistrar(a, token, lx.texto, minusculo, CAT_INSTRUCAO, linha, coluna);
    } else {
        emitirERegistrar(a, "ID", lx.texto, lx.texto, CAT_IDENTIFICADOR, linha, coluna);
    }
}

/* Lê o prefixo ('.' ou '$') e a palavra seguinte. Retorna 0 se não há nada após o prefixo. */
static int lerPalavraComPrefixo(Analisador *a, Lexema *lx, char *minusculo) {
    iniciarLexema(lx);
    consumirPara(a, lx);
    consumirPalavra(a, lx);
    paraMinusculas(lx->texto, minusculo, TAM_LEXEMA);
    return lx->tamanho > 1;
}

/*
 * Lexema iniciado por '.': emite DIR_* somente se for exatamente uma
 * diretiva da lista. Qualquer outro (ponto isolado, nome desconhecido ou
 * diretiva colada a um não-delimitador, como ".text@" e ".data.x") é
 * consumido até o delimitador e registrado como ERRO_DIRETIVA_INVALIDA.
 */
static void reconhecerDiretiva(Analisador *a) {
    int linha = a->linha, coluna = a->coluna;
    Lexema lx;
    char minusculo[TAM_LEXEMA];
    const char *token = NULL;
    int temNome = lerPalavraComPrefixo(a, &lx, minusculo);

    if (!ehDelimitador(espiar(a))) {
        consumirAteDelimitador(a, &lx);
    } else if (temNome) {
        token = buscarTokenReservado(DIRETIVAS, QTD_DIRETIVAS, minusculo);
    }

    if (token == NULL) {
        erro(a, "ERRO_DIRETIVA_INVALIDA", lx.texto, linha, coluna);
        return;
    }
    emitirERegistrar(a, token, lx.texto, minusculo, CAT_DIRETIVA, linha, coluna);
}

/* '$' seguido de nome: emite REG se for registrador MIPS válido, senão ERRO_REGISTRADOR_INVALIDO. */
static void reconhecerRegistrador(Analisador *a) {
    int linha = a->linha, coluna = a->coluna;
    Lexema lx;
    char minusculo[TAM_LEXEMA];

    if (!lerPalavraComPrefixo(a, &lx, minusculo) || !ehRegistradorValido(minusculo)) {
        erro(a, "ERRO_REGISTRADOR_INVALIDO", lx.texto, linha, coluna);
        return;
    }
    emitirERegistrar(a, "REG", lx.texto, minusculo, CAT_REGISTRADOR, linha, coluna);
}

/*
 * Decimal (opcionalmente negativo) ou hexadecimal (0x...). Chamada com um
 * dígito ou com '-' já seguido de dígito.
 * Erros: "0x", "0xG1", "-2abc", "10@" -> ERRO_NUMERO_MALFORMADO;
 *        "2valor" -> ERRO_IDENTIFICADOR_MALFORMADO.
 * Como nos identificadores, um número colado a um não-delimitador é
 * consumido até o delimitador e vira um único erro.
 */
static void reconhecerNumero(Analisador *a) {
    int linha = a->linha, coluna = a->coluna;
    Lexema lx;
    int negativo = (espiar(a) == '-');
    const char *tipoErro = NULL;

    iniciarLexema(&lx);
    if (negativo) {
        consumirPara(a, &lx);
    }

    if (espiar(a) == '0' && (espiarEm(a, 1) == 'x' || espiarEm(a, 1) == 'X')) {
        int qtdDigitosHex = 0;

        consumirPara(a, &lx); /* '0' */
        consumirPara(a, &lx); /* 'x' */
        while (ehDigitoHex(espiar(a))) {
            consumirPara(a, &lx);
            qtdDigitosHex++;
        }
        if (qtdDigitosHex == 0) {
            tipoErro = "ERRO_NUMERO_MALFORMADO";
        }
    } else {
        while (ehDigito(espiar(a))) {
            consumirPara(a, &lx);
        }
        if (!negativo && ehCaractereDePalavra(espiar(a))) {
            tipoErro = "ERRO_IDENTIFICADOR_MALFORMADO"; /* ex.: "2valor" */
        }
    }

    if (!ehDelimitador(espiar(a))) {
        consumirAteDelimitador(a, &lx);
        if (tipoErro == NULL) {
            tipoErro = "ERRO_NUMERO_MALFORMADO";
        }
    }

    if (tipoErro != NULL) {
        erro(a, tipoErro, lx.texto, linha, coluna);
    } else {
        emitirToken(a, "NUM_INT", lx.texto, linha, coluna);
    }
}

static int ehEscapeValido(int c) {
    return c == 'n' || c == 't' || c == '"' || c == '\\' || c == '0';
}

/*
 * Cadeia entre aspas (aspas incluídas no lexema). Escapes válidos:
 * \n \t \" \\ \0. Erros: escape inválido, quebra de linha ou EOF antes
 * da aspa de fechamento. Com escape inválido, o token não é emitido.
 */
static void reconhecerString(Analisador *a) {
    int linha = a->linha, coluna = a->coluna;
    Lexema lx;
    int teveEscapeInvalido = 0;

    iniciarLexema(&lx);
    consumirPara(a, &lx); /* '"' */

    for (;;) {
        int c = espiar(a);

        if (c == EOF) {
            erro(a, "ERRO_STRING_NAO_FECHADA_EOF", lx.texto, linha, coluna);
            return;
        }
        if (c == '\n') { /* não consumido: a quebra de linha segue para o laço principal */
            erro(a, "ERRO_STRING_NAO_FECHADA", lx.texto, linha, coluna);
            return;
        }
        if (c == '"') {
            consumirPara(a, &lx);
            if (!teveEscapeInvalido) {
                emitirToken(a, "STRING", lx.texto, linha, coluna);
            }
            return;
        }
        if (c == '\\') {
            int linhaBarra = a->linha, colunaBarra = a->coluna;
            int seguinte;

            consumirPara(a, &lx);
            seguinte = espiar(a);
            if (seguinte == '\n' || seguinte == EOF) {
                continue; /* a próxima volta registra a cadeia não fechada */
            }
            consumirPara(a, &lx);
            if (!ehEscapeValido(seguinte)) {
                char sequencia[3];
                sequencia[0] = '\\';
                sequencia[1] = (char) seguinte;
                sequencia[2] = '\0';
                erro(a, "ERRO_ESCAPE_INVALIDO", sequencia, linhaBarra, colunaBarra);
                teveEscapeInvalido = 1;
            }
            continue;
        }
        consumirPara(a, &lx);
    }
}

/* Quantos bytes de continuação seguem o byte inicial 'c' de um caractere UTF-8. */
static int continuacoesEsperadasUtf8(int c) {
    if (c >= 0xF0 && c <= 0xF7) return 3;
    if (c >= 0xE0 && c <= 0xEF) return 2;
    if (c >= 0xC0 && c <= 0xDF) return 1;
    return 0;
}

/*
 * Nenhum estado aceita este caractere. Um caractere UTF-8 de vários bytes
 * (ex.: 'é') gera um único erro com o caractere inteiro no lexema.
 */
static void reconhecerCaractereInvalido(Analisador *a) {
    int linha = a->linha, coluna = a->coluna;
    Lexema lx;
    int restantes;

    iniciarLexema(&lx);
    restantes = continuacoesEsperadasUtf8(consumirPara(a, &lx));
    while (restantes-- > 0 && ehContinuacaoUtf8(espiar(a))) {
        consumirPara(a, &lx);
    }
    erro(a, "ERRO_CARACTERE_INVALIDO", lx.texto, linha, coluna);
}

/* Símbolos de um caractere: nome do token ou NULL. */
static const char *tokenDeSimbolo(int c) {
    switch (c) {
        case ',': return "SMB_COM";
        case ':': return "SMB_COL";
        case '(': return "SMB_OPA";
        case ')': return "SMB_CPA";
        default:  return NULL;
    }
}

/* ============================================================
 * PRINCIPAL
 * ============================================================ */

void AnaliseLexica(FILE *in, FILE *out) {
    ResultadoLexico *resultado = &resultadoAtual;
    Analisador a;

    free(resultado->simbolos.itens);
    free(resultado->erros.itens);
    memset(resultado, 0, sizeof(*resultado));
    popularPalavrasReservadas(&resultado->simbolos);

    a.fonte = lerArquivoInteiro(in, &a.tamanho);
    a.pos = 0;
    a.linha = 1;
    a.coluna = 1;
    a.saida = out;
    a.resultado = resultado;

    for (;;) {
        int c = espiar(&a);
        const char *simbolo;

        if (c == EOF) {
            emitirToken(&a, "TK_EOF", "EOF", a.linha, a.coluna);
            break;
        }

        if (c == ' ' || c == '\t' || c == '\n') {
            avancar(&a);
        } else if (c == '#') {
            consumirComentario(&a);
        } else if ((simbolo = tokenDeSimbolo(c)) != NULL) {
            char lexema[2];
            lexema[0] = (char) c;
            lexema[1] = '\0';
            emitirToken(&a, simbolo, lexema, a.linha, a.coluna);
            avancar(&a);
        } else if (ehLetra(c) || c == '_') {
            reconhecerIdentificadorOuInstrucao(&a);
        } else if (c == '.') {
            reconhecerDiretiva(&a);
        } else if (c == '$') {
            reconhecerRegistrador(&a);
        } else if (ehDigito(c) || (c == '-' && ehDigito(espiarEm(&a, 1)))) {
            /* '-' só inicia número se seguido imediatamente de dígito */
            reconhecerNumero(&a);
        } else if (c == '"') {
            reconhecerString(&a);
        } else {
            reconhecerCaractereInvalido(&a);
        }
    }

    free(a.fonte);
}

void GravarTabelaSimbolos(FILE *out) {
    const TabelaSimbolos *ts = &resultadoAtual.simbolos;
    int i;

    fprintf(out, "LEXEMA,CATEGORIA,LINHA,COLUNA\n");
    for (i = 0; i < ts->quantidade; i++) {
        const Simbolo *s = &ts->itens[i];
        fprintf(out, "%s,%s,%d,%d\n", s->lexema, s->categoria, s->primeiraLinha, s->primeiraColuna);
    }
}

void GravarErros(FILE *out) {
    const ListaErros *erros = &resultadoAtual.erros;
    int i;

    if (erros->quantidade == 0) {
        fprintf(out, "Nenhum erro lexico encontrado.\n");
        return;
    }
    for (i = 0; i < erros->quantidade; i++) {
        const Token *e = &erros->itens[i];
        fprintf(out, "<%s, %s> %d %d\n", e->nome, e->lexema, e->linha, e->coluna);
    }
}

int QuantidadeErrosLexicos(void) {
    return resultadoAtual.erros.quantidade;
}
