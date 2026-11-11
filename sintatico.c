#include "sintatico.h"
#include "token.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

/* Categorias que só existem dentro deste módulo ou que ele precisa reconhecer pelo nome. */
#define CAT_EOL "EOL"
#define CAT_EOF "TK_EOF"

/* ============================================================
 * Carga dos tokens do .lex e inserção dos marcadores EOL
 * ============================================================ */

#define TAM_LINHA_LEX 512

typedef struct {
    Token *itens;
    int quantidade;
    int capacidade;
} ListaTokens;

/* Acrescenta um token à lista. Retorna 0 se faltar memória. */
static int acrescentarToken(ListaTokens *lista, const char *nome, const char *lexema,
                            int linha, int coluna) {
    Token *token;

    if (lista->quantidade == lista->capacidade) {
        int novaCapacidade = (lista->capacidade == 0) ? 128 : lista->capacidade * 2;
        Token *itens = (Token *) realloc(lista->itens, sizeof(Token) * (size_t) novaCapacidade);
        if (itens == NULL) {
            return 0;
        }
        lista->itens = itens;
        lista->capacidade = novaCapacidade;
    }

    token = &lista->itens[lista->quantidade++];
    copiarTexto(token->nome, nome, TAM_NOME_TOKEN);
    copiarTexto(token->lexema, lexema, TAM_LEXEMA);
    token->linha = linha;
    token->coluna = coluna;
    return 1;
}

/*
 * Interpreta uma linha "<NOME, lexema> linha coluna" (modificando-a).
 * O lexema pode conter '>' e ", " (strings), por isso o fim é o último '>'
 * da linha e o nome termina no primeiro ", ". Retorna 0 se malformada.
 */
static int interpretarLinhaLex(char *texto, char **nome, char **lexema, int *linha, int *coluna) {
    char *fechamento = strrchr(texto, '>');
    char *separador;

    if (texto[0] != '<' || fechamento == NULL) {
        return 0;
    }
    if (sscanf(fechamento + 1, "%d %d", linha, coluna) != 2) {
        return 0;
    }
    *fechamento = '\0';

    separador = strstr(texto, ", ");
    if (separador == NULL) {
        return 0;
    }
    *separador = '\0';

    *nome = texto + 1;
    *lexema = separador + 2;
    return 1;
}

/* Colunas ocupadas pelo lexema (caracteres, não bytes: ignora continuações UTF-8). */
static int larguraDoLexema(const char *lexema) {
    int largura = 0;
    for (; *lexema != '\0'; lexema++) {
        if (!ehContinuacaoUtf8((unsigned char) *lexema)) {
            largura++;
        }
    }
    return largura;
}

/*
 * Carrega o .lex em 'lista', produzindo os marcadores internos:
 *   - EOL após o último token de cada linha que contém tokens (inclusive a
 *     última, mesmo sem quebra de linha final), posicionado logo após o
 *     último lexema da linha;
 *   - TK_EOF ao final, se o .lex não o trouxer (nunca duplicado).
 * Retorna 0 em caso de falha de leitura, memória ou linha malformada.
 */
static int carregarTokens(FILE *in, ListaTokens *lista) {
    char texto[TAM_LINHA_LEX];
    int haLinhaAberta = 0;      /* há tokens na linha corrente ainda sem EOL */
    int ultimaLinha = 1;
    int colunaAposUltimo = 1;

    while (fgets(texto, sizeof(texto), in) != NULL) {
        size_t tamanho = strlen(texto);
        char *nome, *lexema;
        int linha, coluna;
        int ehEof;

        if (tamanho > 0 && texto[tamanho - 1] != '\n' && !feof(in)) {
            return 0; /* linha maior que qualquer token possível */
        }
        while (tamanho > 0 && (texto[tamanho - 1] == '\n' || texto[tamanho - 1] == '\r')) {
            texto[--tamanho] = '\0';
        }
        if (tamanho == 0) {
            continue;
        }
        if (!interpretarLinhaLex(texto, &nome, &lexema, &linha, &coluna)) {
            return 0;
        }

        ehEof = (strcmp(nome, CAT_EOF) == 0);

        if (haLinhaAberta && (ehEof || linha != ultimaLinha)) {
            if (!acrescentarToken(lista, CAT_EOL, "", ultimaLinha, colunaAposUltimo)) {
                return 0;
            }
            haLinhaAberta = 0;
        }
        if (!acrescentarToken(lista, nome, lexema, linha, coluna)) {
            return 0;
        }
        if (ehEof) {
            return !ferror(in); /* nada depois de TK_EOF interessa */
        }

        haLinhaAberta = 1;
        ultimaLinha = linha;
        colunaAposUltimo = coluna + larguraDoLexema(lexema);
    }

    if (ferror(in)) {
        return 0;
    }

    /* .lex sem TK_EOF explícito: fecha a última linha e acrescenta o marcador */
    if (haLinhaAberta && !acrescentarToken(lista, CAT_EOL, "", ultimaLinha, colunaAposUltimo)) {
        return 0;
    }
    return acrescentarToken(lista, CAT_EOF, "EOF", ultimaLinha, colunaAposUltimo);
}

/* ============================================================
 * Árvore sintática concreta
 * ============================================================ */

typedef struct No No;
struct No {
    const char *naoTerminal;  /* nome do não terminal; NULL nas folhas */
    const Token *token;       /* terminal reconhecido; NULL nos nós internos */
    No *primeiroFilho;
    No *ultimoFilho;
    No *proximoIrmao;
    No *proximoAlocado;       /* encadeia todos os nós para a liberação */
    int id;
};

/* ============================================================
 * Estado do analisador
 * ============================================================ */

typedef struct {
    const Token *tokens;  /* termina sempre em TK_EOF */
    int pos;
    FILE *erros;
    int qtdErros;
    No *alocados;
    No descarte;          /* nó devolvido quando a árvore deixou de ser construída */
    int semMemoria;
} Analisador;

/*
 * Cria um nó (interno se 'token' for NULL, folha caso contrário) como último
 * filho de 'pai'. A árvore só é gravada para programas válidos; por isso,
 * depois do primeiro erro (ou se faltar memória) nenhum nó novo é criado e
 * as funções recebem o nó de descarte, que nunca é ligado à árvore.
 */
static No *novoNo(Analisador *a, No *pai, const char *naoTerminal, const Token *token) {
    No *no;

    if (a->qtdErros > 0 || a->semMemoria) {
        return &a->descarte;
    }
    no = (No *) calloc(1, sizeof(No));
    if (no == NULL) {
        a->semMemoria = 1;
        return &a->descarte;
    }

    no->naoTerminal = naoTerminal;
    no->token = token;
    no->proximoAlocado = a->alocados;
    a->alocados = no;

    if (pai != NULL) {
        if (pai->ultimoFilho != NULL) {
            pai->ultimoFilho->proximoIrmao = no;
        } else {
            pai->primeiroFilho = no;
        }
        pai->ultimoFilho = no;
    }
    return no;
}

static void liberarNos(Analisador *a) {
    while (a->alocados != NULL) {
        No *proximo = a->alocados->proximoAlocado;
        free(a->alocados);
        a->alocados = proximo;
    }
}

/* ============================================================
 * Consulta e consumo de tokens
 * ============================================================ */

static const Token *atual(const Analisador *a) {
    return &a->tokens[a->pos];
}

static int atualEh(const Analisador *a, const char *categoria) {
    return strcmp(atual(a)->nome, categoria) == 0;
}

/* O token atual pertence à lista (terminada em NULL)? */
static int atualEm(const Analisador *a, const char *const *categorias) {
    for (; *categorias != NULL; categorias++) {
        if (atualEh(a, *categorias)) {
            return 1;
        }
    }
    return 0;
}

/* Nunca passa de TK_EOF. */
static void avancar(Analisador *a) {
    if (!atualEh(a, CAT_EOF)) {
        a->pos++;
    }
}

/* ============================================================
 * Diagnóstico e recuperação
 * ============================================================ */

/*
 * Registra "encontrado <token atual>; esperado <esperado>" com a posição do
 * token atual. Para EOL, a posição é a logo após o último lexema da linha.
 * Retorna sempre 0, para ser usada como resultado de uma função de reconhecimento.
 */
static int erroEsperado(Analisador *a, const char *esperado) {
    const Token *t = atual(a);

    fprintf(a->erros, "Linha %d, coluna %d:\nEncontrado ", t->linha, t->coluna);
    if (atualEh(a, CAT_EOL)) {
        fprintf(a->erros, "fim de linha (EOL)");
    } else if (atualEh(a, CAT_EOF)) {
        fprintf(a->erros, "TK_EOF");
    } else if (t->lexema[0] == '"') {
        fprintf(a->erros, "%s(%s)", t->nome, t->lexema); /* a string já traz as aspas */
    } else {
        fprintf(a->erros, "%s(\"%s\")", t->nome, t->lexema);
    }
    fprintf(a->erros, "; esperado %s.\n", esperado);

    a->qtdErros++;
    return 0;
}

/* Como um terminal esperado aparece nas mensagens (os não listados aparecem pelo nome). */
static const struct {
    const char *categoria;
    const char *descricao;
} DESCRICOES_TERMINAIS[] = {
    {"SMB_COM", "SMB_COM(\",\")"},
    {"SMB_COL", "SMB_COL(\":\")"},
    {"SMB_OPA", "SMB_OPA(\"(\")"},
    {"SMB_CPA", "SMB_CPA(\")\")"},
    {CAT_EOL,   "fim de linha (EOL)"}
};

static const char *descreverEsperado(const char *categoria) {
    size_t i;
    for (i = 0; i < sizeof(DESCRICOES_TERMINAIS) / sizeof(DESCRICOES_TERMINAIS[0]); i++) {
        if (strcmp(categoria, DESCRICOES_TERMINAIS[i].categoria) == 0) {
            return DESCRICOES_TERMINAIS[i].descricao;
        }
    }
    return categoria;
}

/* Acrescenta o token atual como folha de 'pai' e avança. Retorna sempre 1. */
static int consumirFolha(Analisador *a, No *pai) {
    novoNo(a, pai, NULL, atual(a));
    avancar(a);
    return 1;
}

/* Consome o terminal 'categoria' como folha de 'pai', ou registra o erro. */
static int esperar(Analisador *a, No *pai, const char *categoria) {
    if (!atualEh(a, categoria)) {
        return erroEsperado(a, descreverEsperado(categoria));
    }
    return consumirFolha(a, pai);
}

/* Consome qualquer terminal da lista como folha de 'pai', ou registra o erro. */
static int esperarUmDe(Analisador *a, No *pai, const char *const *categorias, const char *descricao) {
    if (!atualEm(a, categorias)) {
        return erroEsperado(a, descricao);
    }
    return consumirFolha(a, pai);
}

/* Trecho REG "," comum aos operandos das instruções. */
static int registradorEVirgula(Analisador *a, No *pai) {
    return esperar(a, pai, "REG") && esperar(a, pai, "SMB_COM");
}

/*
 * Modo pânico por linha: descarta o restante da linha corrente, inclusive o
 * EOL, e para. Nunca consome tokens da linha seguinte nem passa de TK_EOF.
 * Se o token atual já for EOL, consome só ele.
 */
static void recuperar(Analisador *a) {
    while (!atualEh(a, CAT_EOL) && !atualEh(a, CAT_EOF)) {
        avancar(a);
    }
    if (atualEh(a, CAT_EOL)) {
        avancar(a);
    }
}

/* ============================================================
 * Conjuntos de terminais usados nas decisões
 * ============================================================ */

static const char *const DIRETIVAS_INTEIROS[] = {"DIR_WORD", "DIR_HALF", "DIR_BYTE", NULL};
static const char *const DIRETIVAS_STRING[]   = {"DIR_ASCII", "DIR_ASCIIZ", NULL};

static const char *const OPS_TRES_REGISTRADORES[] = {
    "INS_ADD", "INS_ADDU", "INS_SUB", "INS_SUBU", "INS_MUL",
    "INS_AND", "INS_OR", "INS_XOR", "INS_NOR", "INS_SLT", NULL
};
static const char *const OPS_DOIS_REGISTRADORES[] = {"INS_MOVE", "INS_DIV", NULL};
static const char *const OPS_UM_REGISTRADOR[]     = {"INS_MFLO", "INS_MFHI", "INS_JR", NULL};
static const char *const OPS_DESLOCAMENTO[]       = {"INS_SLL", "INS_SRL", NULL};
static const char *const OPS_MEMORIA[] = {
    "INS_LW", "INS_SW", "INS_LB", "INS_SB", "INS_LH", "INS_SH", NULL
};
static const char *const OPS_DESVIO[] = {
    "INS_BEQ", "INS_BNE", "INS_BLT", "INS_BLE", "INS_BGT", "INS_BGE", NULL
};
static const char *const OPS_SALTO[]        = {"INS_J", "INS_JAL", NULL};
static const char *const OPS_SEM_OPERANDOS[] = {"INS_SYSCALL", "INS_NOP", NULL};

static int iniciaSecao(const Analisador *a) {
    return atualEh(a, "DIR_DATA") || atualEh(a, "DIR_TEXT");
}

/* Toda instrução do léxico tem categoria INS_*. */
static int iniciaInstrucao(const Analisador *a) {
    return strncmp(atual(a)->nome, "INS_", 4) == 0;
}

static int iniciaDiretivaDados(const Analisador *a) {
    return atualEm(a, DIRETIVAS_INTEIROS) || atualEm(a, DIRETIVAS_STRING)
        || atualEh(a, "DIR_SPACE") || atualEh(a, "DIR_ALIGN");
}

/* ============================================================
 * Funções de reconhecimento: uma por não terminal.
 *
 * As que reconhecem parte de uma linha devolvem 1 (reconhecido) ou 0 (erro
 * já registrado) e não fazem recuperação: quem recupera é a função da linha
 * (<linha_dados>, <linha_codigo>) ou da seção, que descarta até o EOL.
 * ============================================================ */

/* <rotulo> ::= ID ":" */
static int rotulo(Analisador *a, No *pai) {
    No *no = novoNo(a, pai, "<rotulo>", NULL);
    return esperar(a, no, "ID") && esperar(a, no, "SMB_COL");
}

/* <diretiva_global> ::= ".globl" ID */
static int diretivaGlobal(Analisador *a, No *pai) {
    No *no = novoNo(a, pai, "<diretiva_global>", NULL);
    return esperar(a, no, "DIR_GLOBL") && esperar(a, no, "ID");
}

/* ---------- Declarações de dados ---------- */

/* <diretiva_inteiros> ::= ".word" | ".half" | ".byte" */
static int diretivaInteiros(Analisador *a, No *pai) {
    No *no = novoNo(a, pai, "<diretiva_inteiros>", NULL);
    return esperarUmDe(a, no, DIRETIVAS_INTEIROS, ".word, .half ou .byte");
}

/* <lista_inteiros> ::= NUM_INT { "," NUM_INT } */
static int listaInteiros(Analisador *a, No *pai) {
    No *no = novoNo(a, pai, "<lista_inteiros>", NULL);

    if (!esperar(a, no, "NUM_INT")) {
        return 0;
    }
    while (atualEh(a, "SMB_COM")) {
        consumirFolha(a, no);
        if (!esperar(a, no, "NUM_INT")) {
            return 0;
        }
    }
    return 1;
}

/* <diretiva_string> ::= ".ascii" | ".asciiz" */
static int diretivaString(Analisador *a, No *pai) {
    No *no = novoNo(a, pai, "<diretiva_string>", NULL);
    return esperarUmDe(a, no, DIRETIVAS_STRING, ".ascii ou .asciiz");
}

/*
 * <diretiva_dados> ::= <diretiva_inteiros> <lista_inteiros>
 *                    | <diretiva_string> STRING
 *                    | ".space" NUM_INT
 *                    | ".align" NUM_INT
 */
static int diretivaDados(Analisador *a, No *pai) {
    No *no = novoNo(a, pai, "<diretiva_dados>", NULL);

    if (atualEm(a, DIRETIVAS_INTEIROS)) {
        return diretivaInteiros(a, no) && listaInteiros(a, no);
    }
    if (atualEm(a, DIRETIVAS_STRING)) {
        return diretivaString(a, no) && esperar(a, no, "STRING");
    }
    if (atualEh(a, "DIR_SPACE")) {
        return esperar(a, no, "DIR_SPACE") && esperar(a, no, "NUM_INT");
    }
    if (atualEh(a, "DIR_ALIGN")) {
        return esperar(a, no, "DIR_ALIGN") && esperar(a, no, "NUM_INT");
    }
    return erroEsperado(a, "diretiva de dados");
}

/* ---------- Instruções ---------- */

/* <op_tres_registradores> ::= "add" | "addu" | "sub" | "subu" | "mul" | "and" | "or" | "xor" | "nor" | "slt" */
static int opTresRegistradores(Analisador *a, No *pai) {
    No *no = novoNo(a, pai, "<op_tres_registradores>", NULL);
    return esperarUmDe(a, no, OPS_TRES_REGISTRADORES, "operação com três registradores");
}

/* <instrucao_tres_registradores> ::= <op_tres_registradores> REG "," REG "," REG */
static int instrucaoTresRegistradores(Analisador *a, No *pai) {
    No *no = novoNo(a, pai, "<instrucao_tres_registradores>", NULL);
    return opTresRegistradores(a, no)
        && registradorEVirgula(a, no)
        && registradorEVirgula(a, no)
        && esperar(a, no, "REG");
}

/* <op_dois_registradores> ::= "move" | "div" */
static int opDoisRegistradores(Analisador *a, No *pai) {
    No *no = novoNo(a, pai, "<op_dois_registradores>", NULL);
    return esperarUmDe(a, no, OPS_DOIS_REGISTRADORES, "move ou div");
}

/* <instrucao_dois_registradores> ::= <op_dois_registradores> REG "," REG */
static int instrucaoDoisRegistradores(Analisador *a, No *pai) {
    No *no = novoNo(a, pai, "<instrucao_dois_registradores>", NULL);
    return opDoisRegistradores(a, no)
        && registradorEVirgula(a, no)
        && esperar(a, no, "REG");
}

/* <op_um_registrador> ::= "mflo" | "mfhi" | "jr" */
static int opUmRegistrador(Analisador *a, No *pai) {
    No *no = novoNo(a, pai, "<op_um_registrador>", NULL);
    return esperarUmDe(a, no, OPS_UM_REGISTRADOR, "mflo, mfhi ou jr");
}

/* <instrucao_um_registrador> ::= <op_um_registrador> REG */
static int instrucaoUmRegistrador(Analisador *a, No *pai) {
    No *no = novoNo(a, pai, "<instrucao_um_registrador>", NULL);
    return opUmRegistrador(a, no) && esperar(a, no, "REG");
}

/* <instrucao_imediato> ::= "addi" REG "," REG "," NUM_INT */
static int instrucaoImediato(Analisador *a, No *pai) {
    No *no = novoNo(a, pai, "<instrucao_imediato>", NULL);
    return esperar(a, no, "INS_ADDI")
        && registradorEVirgula(a, no)
        && registradorEVirgula(a, no)
        && esperar(a, no, "NUM_INT");
}

/* <op_deslocamento> ::= "sll" | "srl" */
static int opDeslocamento(Analisador *a, No *pai) {
    No *no = novoNo(a, pai, "<op_deslocamento>", NULL);
    return esperarUmDe(a, no, OPS_DESLOCAMENTO, "sll ou srl");
}

/* <instrucao_deslocamento> ::= <op_deslocamento> REG "," REG "," NUM_INT */
static int instrucaoDeslocamento(Analisador *a, No *pai) {
    No *no = novoNo(a, pai, "<instrucao_deslocamento>", NULL);
    return opDeslocamento(a, no)
        && registradorEVirgula(a, no)
        && registradorEVirgula(a, no)
        && esperar(a, no, "NUM_INT");
}

/* <instrucao_li> ::= "li" REG "," NUM_INT */
static int instrucaoLi(Analisador *a, No *pai) {
    No *no = novoNo(a, pai, "<instrucao_li>", NULL);
    return esperar(a, no, "INS_LI")
        && registradorEVirgula(a, no)
        && esperar(a, no, "NUM_INT");
}

/* <instrucao_la> ::= "la" REG "," ID */
static int instrucaoLa(Analisador *a, No *pai) {
    No *no = novoNo(a, pai, "<instrucao_la>", NULL);
    return esperar(a, no, "INS_LA")
        && registradorEVirgula(a, no)
        && esperar(a, no, "ID");
}

/* <op_memoria> ::= "lw" | "sw" | "lb" | "sb" | "lh" | "sh" */
static int opMemoria(Analisador *a, No *pai) {
    No *no = novoNo(a, pai, "<op_memoria>", NULL);
    return esperarUmDe(a, no, OPS_MEMORIA, "lw, sw, lb, sb, lh ou sh");
}

/* <endereco> ::= ID | NUM_INT "(" REG ")" */
static int endereco(Analisador *a, No *pai) {
    No *no = novoNo(a, pai, "<endereco>", NULL);

    if (atualEh(a, "ID")) {
        return esperar(a, no, "ID");
    }
    if (atualEh(a, "NUM_INT")) {
        return esperar(a, no, "NUM_INT") && esperar(a, no, "SMB_OPA")
            && esperar(a, no, "REG") && esperar(a, no, "SMB_CPA");
    }
    return erroEsperado(a, "endereço (ID ou NUM_INT)");
}

/* <instrucao_memoria> ::= <op_memoria> REG "," <endereco> */
static int instrucaoMemoria(Analisador *a, No *pai) {
    No *no = novoNo(a, pai, "<instrucao_memoria>", NULL);
    return opMemoria(a, no)
        && registradorEVirgula(a, no)
        && endereco(a, no);
}

/* <op_desvio> ::= "beq" | "bne" | "blt" | "ble" | "bgt" | "bge" */
static int opDesvio(Analisador *a, No *pai) {
    No *no = novoNo(a, pai, "<op_desvio>", NULL);
    return esperarUmDe(a, no, OPS_DESVIO, "beq, bne, blt, ble, bgt ou bge");
}

/* <instrucao_desvio> ::= <op_desvio> REG "," REG "," ID */
static int instrucaoDesvio(Analisador *a, No *pai) {
    No *no = novoNo(a, pai, "<instrucao_desvio>", NULL);
    return opDesvio(a, no)
        && registradorEVirgula(a, no)
        && registradorEVirgula(a, no)
        && esperar(a, no, "ID");
}

/* <op_salto> ::= "j" | "jal" */
static int opSalto(Analisador *a, No *pai) {
    No *no = novoNo(a, pai, "<op_salto>", NULL);
    return esperarUmDe(a, no, OPS_SALTO, "j ou jal");
}

/* <instrucao_salto> ::= <op_salto> ID */
static int instrucaoSalto(Analisador *a, No *pai) {
    No *no = novoNo(a, pai, "<instrucao_salto>", NULL);
    return opSalto(a, no) && esperar(a, no, "ID");
}

/* <instrucao_sem_operandos> ::= "syscall" | "nop" */
static int instrucaoSemOperandos(Analisador *a, No *pai) {
    No *no = novoNo(a, pai, "<instrucao_sem_operandos>", NULL);
    return esperarUmDe(a, no, OPS_SEM_OPERANDOS, "syscall ou nop");
}

/*
 * <instrucao> ::= <instrucao_tres_registradores> | <instrucao_dois_registradores>
 *               | <instrucao_um_registrador> | <instrucao_imediato>
 *               | <instrucao_deslocamento> | <instrucao_li> | <instrucao_la>
 *               | <instrucao_memoria> | <instrucao_desvio> | <instrucao_salto>
 *               | <instrucao_sem_operandos>
 * A alternativa é escolhida pelo token da operação.
 */
static int instrucao(Analisador *a, No *pai) {
    No *no = novoNo(a, pai, "<instrucao>", NULL);

    if (atualEm(a, OPS_TRES_REGISTRADORES)) return instrucaoTresRegistradores(a, no);
    if (atualEm(a, OPS_DOIS_REGISTRADORES)) return instrucaoDoisRegistradores(a, no);
    if (atualEm(a, OPS_UM_REGISTRADOR))     return instrucaoUmRegistrador(a, no);
    if (atualEh(a, "INS_ADDI"))             return instrucaoImediato(a, no);
    if (atualEm(a, OPS_DESLOCAMENTO))       return instrucaoDeslocamento(a, no);
    if (atualEh(a, "INS_LI"))               return instrucaoLi(a, no);
    if (atualEh(a, "INS_LA"))               return instrucaoLa(a, no);
    if (atualEm(a, OPS_MEMORIA))            return instrucaoMemoria(a, no);
    if (atualEm(a, OPS_DESVIO))             return instrucaoDesvio(a, no);
    if (atualEm(a, OPS_SALTO))              return instrucaoSalto(a, no);
    if (atualEm(a, OPS_SEM_OPERANDOS))      return instrucaoSemOperandos(a, no);
    return erroEsperado(a, "instrução");
}

/* ---------- Linhas e seções ---------- */

/*
 * As seções .data e .text têm a mesma forma e diferem só no conteúdo
 * permitido em cada linha; este descritor reúne essas diferenças.
 */
typedef struct {
    const char *abertura;                           /* DIR_DATA ou DIR_TEXT */
    const char *naoTerminalSecao;
    const char *naoTerminalLinha;
    int (*iniciaConteudo)(const Analisador *);      /* o token atual inicia o conteúdo? */
    int (*conteudo)(Analisador *, No *);            /* <diretiva_dados> ou <instrucao> */
    const char *esperadoAposRotulo;
    const char *esperadoNaLinha;
} TipoSecao;

static const TipoSecao SECAO_DADOS = {
    "DIR_DATA", "<secao_dados>", "<linha_dados>",
    iniciaDiretivaDados, diretivaDados,
    "diretiva de dados ou fim de linha (EOL)",
    "rótulo, diretiva de dados ou .globl (seção .data)"
};

static const TipoSecao SECAO_CODIGO = {
    "DIR_TEXT", "<secao_codigo>", "<linha_codigo>",
    iniciaInstrucao, instrucao,
    "instrução ou fim de linha (EOL)",
    "rótulo, instrução ou .globl (seção .text)"
};

/*
 * <linha_X> ::= <rotulo> [ <conteudo> ] EOL | <conteudo> EOL | <diretiva_global> EOL
 * Qualquer erro na linha é recuperado aqui, descartando-a até o EOL.
 */
static void linhaDaSecao(Analisador *a, No *pai, const TipoSecao *tipo) {
    No *no = novoNo(a, pai, tipo->naoTerminalLinha, NULL);
    int ok;

    if (atualEh(a, "ID")) {
        ok = rotulo(a, no);
        if (ok && tipo->iniciaConteudo(a)) {
            ok = tipo->conteudo(a, no);
        } else if (ok && !atualEh(a, CAT_EOL)) {
            ok = erroEsperado(a, tipo->esperadoAposRotulo);
        }
    } else if (tipo->iniciaConteudo(a)) {
        ok = tipo->conteudo(a, no);
    } else if (atualEh(a, "DIR_GLOBL")) {
        ok = diretivaGlobal(a, no);
    } else {
        ok = erroEsperado(a, tipo->esperadoNaLinha);
    }

    if (!ok || !esperar(a, no, CAT_EOL)) {
        recuperar(a);
    }
}

/*
 * <secao_X> ::= abertura EOL { <linha_X> }
 * A repetição só termina na abertura de outra seção ou em TK_EOF; uma
 * construção não permitida aqui é erro de <linha_X>, não fim da seção.
 */
static void secaoDoTipo(Analisador *a, No *pai, const TipoSecao *tipo,
                        void (*linha)(Analisador *, No *)) {
    No *no = novoNo(a, pai, tipo->naoTerminalSecao, NULL);

    esperar(a, no, tipo->abertura);
    if (!esperar(a, no, CAT_EOL)) {
        recuperar(a); /* a seção continua ativa para as linhas seguintes */
    }
    while (!iniciaSecao(a) && !atualEh(a, CAT_EOF)) {
        linha(a, no);
    }
}

/*
 * <linha_dados> ::= <rotulo> [ <diretiva_dados> ] EOL
 *                 | <diretiva_dados> EOL
 *                 | <diretiva_global> EOL
 */
static void linhaDados(Analisador *a, No *pai) {
    linhaDaSecao(a, pai, &SECAO_DADOS);
}

/*
 * <linha_codigo> ::= <rotulo> [ <instrucao> ] EOL
 *                  | <instrucao> EOL
 *                  | <diretiva_global> EOL
 */
static void linhaCodigo(Analisador *a, No *pai) {
    linhaDaSecao(a, pai, &SECAO_CODIGO);
}

/* <secao_dados> ::= ".data" EOL { <linha_dados> } */
static void secaoDados(Analisador *a, No *pai) {
    secaoDoTipo(a, pai, &SECAO_DADOS, linhaDados);
}

/* <secao_codigo> ::= ".text" EOL { <linha_codigo> } */
static void secaoCodigo(Analisador *a, No *pai) {
    secaoDoTipo(a, pai, &SECAO_CODIGO, linhaCodigo);
}

/*
 * <secao> ::= <secao_dados> | <secao_codigo>
 * Fora de qualquer seção (antes da primeira abertura), cada linha é um erro
 * e é descartada sozinha, sem consumir as linhas seguintes.
 */
static void secao(Analisador *a, No *pai) {
    No *no;

    if (!iniciaSecao(a)) {
        erroEsperado(a, "abertura de seção (.data ou .text)");
        recuperar(a);
        return;
    }

    no = novoNo(a, pai, "<secao>", NULL);
    if (atualEh(a, "DIR_DATA")) {
        secaoDados(a, no);
    } else {
        secaoCodigo(a, no);
    }
}

/*
 * <programa> ::= <secao> { <secao> } TK_EOF
 * A primeira <secao> é obrigatória: arquivo vazio ou só com comentários
 * gera erro em TK_EOF.
 */
static No *programa(Analisador *a) {
    No *no = novoNo(a, NULL, "<programa>", NULL);

    secao(a, no);
    while (!atualEh(a, CAT_EOF)) {
        secao(a, no);
    }
    esperar(a, no, CAT_EOF);
    return no;
}

/* ============================================================
 * Gravação da árvore em DOT
 * ============================================================ */

/* Escreve 'texto' dentro de um rótulo DOT entre aspas, escapando '"' e '\'. */
static void escreverEscapado(FILE *out, const char *texto) {
    for (; *texto != '\0'; texto++) {
        if (*texto == '"' || *texto == '\\') {
            fputc('\\', out);
        }
        fputc(*texto, out);
    }
}

/* Declara os nós em pré-ordem, numerando-os; cada ocorrência recebe um id exclusivo. */
static void escreverNos(FILE *out, No *no, int *proximoId) {
    No *filho;

    no->id = (*proximoId)++;
    fprintf(out, "n%d [label=\"", no->id);
    if (no->token == NULL) {
        escreverEscapado(out, no->naoTerminal);
    } else {
        escreverEscapado(out, no->token->nome);
        /* EOL e TK_EOF não têm lexema a mostrar */
        if (strcmp(no->token->nome, CAT_EOL) != 0 && strcmp(no->token->nome, CAT_EOF) != 0) {
            fputs("\\n", out);
            escreverEscapado(out, no->token->lexema);
        }
    }
    fputs("\"];\n", out);

    for (filho = no->primeiroFilho; filho != NULL; filho = filho->proximoIrmao) {
        escreverNos(out, filho, proximoId);
    }
}

/* Arestas pai -> filho, na ordem dos símbolos da produção. */
static void escreverArestas(FILE *out, const No *no) {
    const No *filho;

    for (filho = no->primeiroFilho; filho != NULL; filho = filho->proximoIrmao) {
        fprintf(out, "n%d -> n%d;\n", no->id, filho->id);
    }
    for (filho = no->primeiroFilho; filho != NULL; filho = filho->proximoIrmao) {
        escreverArestas(out, filho);
    }
}

static void escreverArvore(FILE *out, No *raiz) {
    int proximoId = 0;

    fputs("digraph ArvoreSintatica {\n", out);
    fputs("graph [ordering=out];\n", out);
    fputs("node [shape=box, fontname=\"Arial\"];\n", out);
    fputs("edge [arrowhead=none];\n", out);
    escreverNos(out, raiz, &proximoId);
    escreverArestas(out, raiz);
    fputs("}\n", out);
}

/* ============================================================
 * PRINCIPAL
 * ============================================================ */

int AnaliseSintatica(FILE *tokens, FILE *arvore, FILE *erros) {
    ListaTokens lista = {NULL, 0, 0};
    Analisador a;
    No *raiz;
    int resultado;

    if (tokens == NULL || arvore == NULL || erros == NULL) {
        return SINTATICO_FALHA;
    }
    if (!carregarTokens(tokens, &lista)) {
        free(lista.itens);
        return SINTATICO_FALHA;
    }

    memset(&a, 0, sizeof(a));
    a.tokens = lista.itens;
    a.erros = erros;

    raiz = programa(&a);

    if (a.semMemoria) {
        resultado = SINTATICO_FALHA;
    } else if (a.qtdErros > 0) {
        resultado = SINTATICO_INVALIDO; /* o .dot permanece vazio */
    } else {
        fputs("Nenhum erro sintático encontrado.\n", erros);
        escreverArvore(arvore, raiz);
        resultado = SINTATICO_VALIDO;
    }

    if (ferror(arvore) || ferror(erros)) {
        resultado = SINTATICO_FALHA;
    }

    liberarNos(&a);
    free(lista.itens);
    return resultado;
}
