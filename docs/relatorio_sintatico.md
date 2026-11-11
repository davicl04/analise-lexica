# Relatório Técnico — Analisador Sintático para Assembly MIPS (subconjunto MARS)

Este relatório descreve a terceira etapa do projeto: o analisador sintático, implementado em C pela técnica de análise descendente recursiva, sem o uso de geradores de analisadores. Ele consome os tokens produzidos pelo analisador léxico, verifica se o programa obedece à gramática da linguagem, registra os erros sintáticos e, para programas válidos, grava a árvore sintática concreta em DOT.

## 1. Organização do projeto

| Arquivo | Conteúdo |
|---|---|
| `main.c` | Valida os argumentos, abre e fecha todos os arquivos e coordena as três etapas. |
| `preprocessador.c` / `.h` | Remove comentários, linhas vazias e espaços redundantes; gera o `.pre`. |
| `lexico.c` / `.h` | Lê o `.pre` e gera o `.lex`, a tabela de símbolos e os erros léxicos. |
| `sintatico.c` / `.h` | Lê o `.lex` e gera o `.sin.err` e o `.dot`. |
| `token.h` | Estruturas `Token` e `Simbolo`, compartilhadas pelos módulos. |
| `util.h` | Funções auxiliares `static inline` (cópia segura de texto, teste de byte UTF-8) usadas pelo léxico e pelo sintático; por ser só cabeçalho, não altera o comando de compilação. |

As interfaces públicas das etapas anteriores foram mantidas: `PreProcessamento(FILE*, FILE*)`, `AnaliseLexica(FILE*, FILE*)`, `GravarTabelaSimbolos(FILE*)` e `GravarErros(FILE*)`. Foi acrescentada ao módulo léxico uma única função de consulta, `int QuantidadeErrosLexicos(void)`, que devolve o número de erros da última análise. É por ela que `main()` decide se a análise sintática pode ser executada.

A etapa sintática expõe a interface exigida:

```c
int AnaliseSintatica(FILE *tokens, FILE *arvore, FILE *erros);
```

| Retorno | Significado |
|---|---|
| `0` | Programa sintaticamente válido. |
| `1` | Programa sintaticamente inválido. |
| `-1` | Falha operacional: erro de leitura ou gravação, falta de memória ou `.lex` malformado. |

O módulo usa apenas os arquivos recebidos por parâmetro; não abre nem fecha nenhum arquivo.

## 2. Compilação e execução

```bash
gcc main.c preprocessador.c lexico.c sintatico.c -o main.exe
./main.exe <arquivo_entrada.asm> <prefixo_saida>
```

O código também compila sem avisos com `-std=c99 -Wall -Wextra -pedantic`.

Para `./main.exe teste.asm resultado` são gerados:

| Arquivo | Conteúdo |
|---|---|
| `resultado.pre` | Código resultante do pré-processamento. |
| `resultado.lex` | Tokens, no formato `<CATEGORIA, lexema> linha coluna`. |
| `resultado.ts` | Tabela de símbolos da etapa léxica. |
| `resultado.lex.err` | Diagnósticos léxicos. |
| `resultado.sin.err` | Diagnósticos sintáticos, ou a mensagem de ausência de erros. |
| `resultado.dot` | Árvore sintática concreta, somente se o programa for válido. |

Todos os arquivos de saída são criados (ou truncados) logo no início da execução, de modo que nenhum resultado de uma execução anterior permaneça. A ordem das etapas é:

1. Pré-processamento do `.asm` para o `.pre`.
2. Análise léxica, lendo o `.pre` desde o início.
3. Análise sintática, lendo o `.lex` desde o início, apenas se não houver erros léxicos.

Havendo erros léxicos, o programa imprime `ANÁLISE SINTÁTICA NÃO EXECUTADA: EXISTEM ERROS LÉXICOS.`, o `.dot` permanece vazio e o `.sin.err` registra o motivo. Nos demais casos o terminal apresenta `PROGRAMA SINTATICAMENTE VÁLIDO` ou `PROGRAMA SINTATICAMENTE INVÁLIDO`.

| Código de encerramento | Situação |
|---|---|
| `0` | Execução concluída e programa sintaticamente válido. |
| `1` | Rejeição por erros léxicos ou sintáticos. |
| `2` | Falha operacional (abertura, leitura, gravação, memória) ou uso incorreto da linha de comando. |

Como o léxico lê o `.pre`, **a linha e a coluna dos tokens e dos diagnósticos referem-se ao arquivo `.pre`**, e não ao `.asm` original. No `.pre` não há comentários, linhas vazias nem espaços iniciais.

Para renderizar uma árvore:

```bash
dot -Tpng resultado.dot -o resultado.png
```

## 3. Estruturas de dados

**Token** (`token.h`). É a mesma estrutura da etapa léxica: categoria (`nome`), `lexema`, `linha` e `coluna`. O módulo sintático carrega todos os tokens do `.lex` em um vetor dinâmico (`ListaTokens`) antes de iniciar o reconhecimento, preservando categoria, lexema e localização de cada um.

**Nó da árvore** (`No`, em `sintatico.c`):

```c
struct No {
    const char *naoTerminal;  /* nome do não terminal; NULL nas folhas */
    const Token *token;       /* terminal reconhecido; NULL nos nós internos */
    No *primeiroFilho;
    No *ultimoFilho;
    No *proximoIrmao;
    No *proximoAlocado;       /* encadeia todos os nós para a liberação */
    int id;
};
```

Os filhos formam uma lista encadeada (`primeiroFilho` / `proximoIrmao`), na ordem em que são criados, que é a ordem dos símbolos na produção. Um nó interno guarda o nome do não terminal; uma folha aponta para o token reconhecido.

**Estado do analisador** (`Analisador`): o vetor de tokens e a posição do token atual, o arquivo de erros e o contador de erros, a lista de nós alocados, um nó de descarte e um indicador de falta de memória. Não há variáveis globais no módulo.

## 4. Correspondência entre não terminais e funções

Cada não terminal da gramática tem uma função em `sintatico.c`. A produção correspondente está no comentário de cada função.

| Não terminal | Função |
|---|---|
| `<programa>` | `programa` |
| `<secao>` | `secao` |
| `<secao_dados>` | `secaoDados` |
| `<secao_codigo>` | `secaoCodigo` |
| `<linha_dados>` | `linhaDados` |
| `<linha_codigo>` | `linhaCodigo` |
| `<rotulo>` | `rotulo` |
| `<diretiva_global>` | `diretivaGlobal` |
| `<diretiva_dados>` | `diretivaDados` |
| `<diretiva_inteiros>` | `diretivaInteiros` |
| `<lista_inteiros>` | `listaInteiros` |
| `<diretiva_string>` | `diretivaString` |
| `<instrucao>` | `instrucao` |
| `<instrucao_tres_registradores>` | `instrucaoTresRegistradores` |
| `<op_tres_registradores>` | `opTresRegistradores` |
| `<instrucao_dois_registradores>` | `instrucaoDoisRegistradores` |
| `<op_dois_registradores>` | `opDoisRegistradores` |
| `<instrucao_um_registrador>` | `instrucaoUmRegistrador` |
| `<op_um_registrador>` | `opUmRegistrador` |
| `<instrucao_imediato>` | `instrucaoImediato` |
| `<instrucao_deslocamento>` | `instrucaoDeslocamento` |
| `<op_deslocamento>` | `opDeslocamento` |
| `<instrucao_li>` | `instrucaoLi` |
| `<instrucao_la>` | `instrucaoLa` |
| `<instrucao_memoria>` | `instrucaoMemoria` |
| `<op_memoria>` | `opMemoria` |
| `<endereco>` | `endereco` |
| `<instrucao_desvio>` | `instrucaoDesvio` |
| `<op_desvio>` | `opDesvio` |
| `<instrucao_salto>` | `instrucaoSalto` |
| `<op_salto>` | `opSalto` |
| `<instrucao_sem_operandos>` | `instrucaoSemOperandos` |

As decisões são sempre tomadas pelo próximo token, sem retrocesso:

- **Alternativas.** `secao` escolhe entre `.data` e `.text`; `instrucao` escolhe o grupo pela categoria da operação (`INS_ADD` leva a `instrucaoTresRegistradores`, `INS_LI` a `instrucaoLi`, e assim por diante); `endereco` escolhe entre `ID` e `NUM_INT`.
- **Partes opcionais.** Em `linhaDados` e `linhaCodigo`, depois de um rótulo, a diretiva ou instrução só é reconhecida se o próximo token puder iniciá-la.
- **Repetições.** As repetições `{ ... }` da EBNF são laços `while`: em `listaInteiros`, enquanto o próximo token for vírgula; em `secaoDados` e `secaoCodigo`, enquanto o próximo token não abrir outra seção nem for `TK_EOF`.

Três funções auxiliares concentram o consumo de terminais: `esperar` (consome o terminal de uma categoria ou registra o erro), `esperarUmDe` (o mesmo para um conjunto de categorias) e `erroEsperado` (registra o diagnóstico). Uma produção sequencial é escrita como uma cadeia de `&&`, que para no primeiro erro:

```c
/* <instrucao_li> ::= "li" REG "," NUM_INT */
static int instrucaoLi(Analisador *a, No *pai) {
    No *no = novoNo(a, pai, "<instrucao_li>", NULL);
    return esperar(a, no, "INS_LI")
        && esperar(a, no, "REG") && esperar(a, no, "SMB_COM")
        && esperar(a, no, "NUM_INT");
}
```

## 5. Tratamento de `EOL` e `TK_EOF`

O `.lex` não contém marcadores de fim de linha, e seu formato não foi alterado. O marcador `EOL` é produzido internamente pela função `carregarTokens`, a partir da linha de cada token:

- Ao ler um token cuja linha é diferente da linha do token anterior, é inserido um `EOL` antes dele.
- Ao ler `TK_EOF`, é inserido um `EOL` antes dele, se houver tokens na última linha. Isso cobre o arquivo sem quebra de linha final.
- Linhas vazias e linhas só com comentário não têm tokens, portanto não produzem `EOL`.
- O `EOL` recebe a linha do último token e a coluna imediatamente posterior ao seu lexema. É essa posição que aparece nos diagnósticos de elemento ausente no fim da linha.

Quanto ao `TK_EOF`: se o `.lex` já traz o token, ele é usado e a leitura termina ali, sem duplicação. Se não traz, o módulo acrescenta um `TK_EOF` ao final. A função `avancar` nunca ultrapassa o `TK_EOF`, de modo que o token atual é sempre válido.

A função `programa` consome a entrada inteira: reconhece as seções enquanto o token atual não for `TK_EOF` e, ao final, consome o `TK_EOF`. O programa só é aceito se nenhum erro tiver sido registrado.

## 6. Diagnóstico e recuperação de erros

**Diagnóstico.** Todo erro é registrado por `erroEsperado`, com a linha e a coluna do token atual, sua categoria e seu lexema, e o elemento esperado:

```
Linha 6, coluna 9:
Encontrado REG("$t1"); esperado SMB_COM(",").
```

- Quando falta um elemento no fim da linha, o token encontrado é o `EOL`: `Encontrado fim de linha (EOL); esperado NUM_INT.`
- No fim do arquivo: `Encontrado TK_EOF; esperado abertura de seção (.data ou .text).`
- O elemento esperado pode ser um terminal (`REG`, `SMB_COM(",")`) ou uma construção (`endereço (ID ou NUM_INT)`, `rótulo, instrução ou .globl (seção .text)`).

Sem erros, o `.sin.err` contém apenas `Nenhum erro sintático encontrado.`

**Recuperação.** É feita por linha, em modo pânico. As funções que reconhecem parte de uma linha devolvem 0 ao encontrar um erro e não tentam se recuperar. Quem recupera é a função da linha (`linhaDados` ou `linhaCodigo`), chamando `recuperar`:

```c
static void recuperar(Analisador *a) {
    while (!atualEh(a, CAT_EOL) && !atualEh(a, CAT_EOF)) {
        avancar(a);
    }
    if (atualEh(a, CAT_EOL)) {
        avancar(a);
    }
}
```

- Os tokens restantes da linha são descartados até o `EOL`, inclusive, e a análise recomeça na linha seguinte.
- Se o token atual já é o `EOL`, só ele é consumido.
- Se é `TK_EOF`, nada é consumido.
- Nenhum token de uma linha posterior é descartado.
- A seção ativa é preservada, porque a recuperação acontece dentro do laço de `secaoDados` ou `secaoCodigo`.
- Toda linha com erro consome ao menos um token, o que garante o avanço e impede a repetição indefinida do mesmo diagnóstico.

É registrado um erro por linha: o primeiro encontrado.

**Seções.** O laço de linhas de uma seção só termina na abertura de outra seção ou em `TK_EOF`. Uma instrução dentro de `.data`, ou uma diretiva de dados dentro de `.text`, é diagnosticada como erro da linha, e não como fim da seção. Se `.data` ou `.text` vier seguido de outros tokens na mesma linha, o erro é registrado, a linha é descartada e a seção passa a valer normalmente para as linhas seguintes.

**Antes da primeira seção.** Linhas que aparecem antes de qualquer abertura de seção não pertencem a nenhuma seção. Cada uma gera o diagnóstico `esperado abertura de seção (.data ou .text)` e é descartada isoladamente. Um arquivo vazio, ou só com comentários, gera esse mesmo erro em `TK_EOF`.

## 7. Construção da árvore sintática concreta

A árvore é construída durante o reconhecimento. Cada função de não terminal começa criando o seu nó como último filho do nó recebido do chamador (`novoNo`), e cada terminal consumido por `esperar` vira uma folha do nó corrente. Assim:

- a raiz é `<programa>`;
- os nós internos são os não terminais, e as folhas são os terminais, incluindo vírgulas, dois-pontos, parênteses, `EOL` e `TK_EOF`;
- os filhos ficam na ordem dos símbolos da produção;
- as repetições aparecem como filhos sucessivos do mesmo nó (por exemplo, cada `<linha_codigo>` é filha direta de `<secao_codigo>`);
- uma parte opcional ausente não gera nó, porque a função correspondente não chega a ser chamada.

**Programas inválidos.** A árvore só é gravada se não houver nenhum erro. A partir do primeiro erro (ou de uma falha de alocação), `novoNo` deixa de alocar e devolve um nó de descarte, que nunca é ligado à árvore. Nesse caso nada é escrito no `.dot`, que permanece vazio; nunca é apresentada uma árvore parcial.

**Gravação em DOT.** `escreverArvore` grava o cabeçalho, depois os nós e depois as arestas:

- os nós são declarados em pré-ordem e numerados nessa ordem (`n0`, `n1`, ...), o que dá a cada ocorrência de símbolo um identificador exclusivo;
- o rótulo de um nó interno é o nome do não terminal; o de uma folha é a categoria e, na linha de baixo, o lexema; `EOL` e `TK_EOF` mostram só a categoria;
- aspas e barras invertidas dos lexemas são escapadas. A string `"Resultado: \"ok\"\n"` é gravada como `label="STRING\n\"Resultado: \\\"ok\\\"\\n\""`;
- o atributo `ordering=out` faz o Graphviz respeitar a ordem dos filhos.

Para o programa de exemplo do enunciado (`V01`), o `.dot` gerado tem 43 nós e 42 arestas, com a mesma estrutura e a mesma numeração do exemplo.

**Imagem da árvore.** A figura abaixo deve ser gerada com:

```bash
dot -Tpng testes/sintatico/saidas/V01_exemplo_sem_quebra_final.dot -o docs/arvore_V01.png
```

![Árvore sintática concreta do programa V01](arvore_V01.png)

## 8. Casos de teste

Os programas de teste estão em `testes/sintatico/entradas/` e todas as saídas obtidas (`.pre`, `.lex`, `.ts`, `.lex.err`, `.sin.err`, `.dot`) estão em `testes/sintatico/saidas/`. Todos os oito programas são lexicalmente válidos (`.lex.err` com `Nenhum erro lexico encontrado.`). Em todos os casos o resultado obtido foi igual ao esperado.

### 8.1. Resumo

| Caso | Esperado | Obtido | Código | Erros | `.dot` |
|---|---|---|---|---|---|
| `V01_exemplo_sem_quebra_final` | Válido | Válido | 0 | 0 | 43 nós, 42 arestas |
| `V02_completo` | Válido | Válido | 0 | 0 | 504 nós, 503 arestas |
| `V03_alternancia_e_limites` | Válido | Válido | 0 | 0 | 95 nós, 94 arestas |
| `E01_vazio` | Inválido | Inválido | 1 | 1 | vazio |
| `E02_sem_secao` | Inválido | Inválido | 1 | 2 | vazio |
| `E03_secao_incorreta` | Inválido | Inválido | 1 | 5 | vazio |
| `E04_operandos` | Inválido | Inválido | 1 | 18 | vazio |
| `E05_pontuacao` | Inválido | Inválido | 1 | 13 | vazio |

### 8.2. Cobertura

| Categoria | Onde é testada |
|---|---|
| Organização | `V01` começa diretamente por `.text`; `V02` tem `.data` e `.text`; `V03` alterna seções e tem seções vazias. |
| Rótulos | `V01` e `V02`: rótulo isolado (`main:`, `isolado:`, `fim:`) e rótulo seguido de instrução (`laco: beq ...`) ou de diretiva (`numero: .word 10`). |
| Diretivas | `V02`: `.data`, `.text`, `.word`, `.half`, `.byte`, `.space`, `.ascii`, `.asciiz`, `.globl` (nas duas seções) e `.align`. |
| Instruções | `V02`: os onze grupos e as 36 operações da gramática. |
| Operandos | `V02`: decimais (`10`, `-30`), hexadecimais (`0xFF`, `0x1F`), strings com escapes, listas de inteiros e referências a identificadores. |
| Memória | `V02`: `lw $t6, numero` e `sw $t6, 4($sp)`. |
| Linhas | `V02` e `V03`: linhas vazias e comentários; `V01`: arquivo sem quebra de linha final. |
| Entrada inválida | `E01`: arquivo vazio; `E02`: programa sem abertura de seção. |
| Seção incorreta | `E03`: instrução em `.data` e diretiva de dados em `.text`. |
| Estrutura incorreta | `E04`: operandos ausentes, excedentes, fora de ordem e de categoria inadequada. |
| Pontuação | `E05`: vírgulas, dois-pontos e parênteses ausentes; lista de inteiros terminada por vírgula. |
| Consumo completo | `E04`: `syscall $t0`, `li $t0, 1 nop`, `add $t0, $t1, $t2, $t3`; `E05`: `.data extra`. |
| Recuperação | `E03`, `E04` e `E05`: erros em linhas distintas, com linhas corretas reconhecidas entre eles e depois deles. |
| Limites da etapa | `V03`: rótulo inexistente (`j nao_existe`), rótulo duplicado (`x:` e `inicio:`), `.space -5`, `.align 99`, `sll` com deslocamento 500 e `syscall` sem preparar `$v0`. Nenhum é rejeitado. |

### 8.3. Diagnósticos obtidos

As posições referem-se ao `.pre` de cada caso.

**`E01_vazio`**

```
Linha 1, coluna 1:
Encontrado TK_EOF; esperado abertura de seção (.data ou .text).
```

**`E02_sem_secao`** — as duas linhas antes de `.text` são rejeitadas; a seção `.text` seguinte é reconhecida.

```
Linha 1, coluna 1:
Encontrado ID("main"); esperado abertura de seção (.data ou .text).
Linha 2, coluna 1:
Encontrado INS_LI("li"); esperado abertura de seção (.data ou .text).
```

**`E03_secao_incorreta`**

```
Linha 3, coluna 1:
Encontrado INS_LI("li"); esperado rótulo, diretiva de dados ou .globl (seção .data).
Linha 4, coluna 6:
Encontrado INS_ADD("add"); esperado diretiva de dados ou fim de linha (EOL).
Linha 8, coluna 1:
Encontrado DIR_WORD(".word"); esperado rótulo, instrução ou .globl (seção .text).
Linha 9, coluna 7:
Encontrado DIR_ASCIIZ(".asciiz"); esperado instrução ou fim de linha (EOL).
Linha 10, coluna 1:
Encontrado DIR_SPACE(".space"); esperado rótulo, instrução ou .globl (seção .text).
```

**`E04_operandos`**

| Linha do `.pre` | Diagnóstico |
|---|---|
| `add $t0, $t1` | Linha 3, coluna 13: encontrado fim de linha (EOL); esperado `SMB_COM(",")`. |
| `li $t0, mensagem` | Linha 4, coluna 9: encontrado `ID("mensagem")`; esperado `NUM_INT`. |
| `syscall $t0` | Linha 5, coluna 9: encontrado `REG("$t0")`; esperado fim de linha (EOL). |
| `add $t0, $t1, $t2, $t3` | Linha 6, coluna 18: encontrado `SMB_COM(",")`; esperado fim de linha (EOL). |
| `addi $t0, 5, $t1` | Linha 7, coluna 11: encontrado `NUM_INT("5")`; esperado `REG`. |
| `la $a0, 10` | Linha 8, coluna 9: encontrado `NUM_INT("10")`; esperado `ID`. |
| `jr fim` | Linha 9, coluna 4: encontrado `ID("fim")`; esperado `REG`. |
| `j $ra` | Linha 10, coluna 3: encontrado `REG("$ra")`; esperado `ID`. |
| `beq $t0, $t1, 4` | Linha 11, coluna 15: encontrado `NUM_INT("4")`; esperado `ID`. |
| `move $t0` | Linha 12, coluna 9: encontrado fim de linha (EOL); esperado `SMB_COM(",")`. |
| `lw $t0, $t1` | Linha 13, coluna 9: encontrado `REG("$t1")`; esperado endereço (ID ou NUM_INT). |
| `li $t0, 1 nop` | Linha 14, coluna 11: encontrado `INS_NOP("nop")`; esperado fim de linha (EOL). |
| `mflo` | Linha 15, coluna 5: encontrado fim de linha (EOL); esperado `REG`. |
| `.word` | Linha 18, coluna 6: encontrado fim de linha (EOL); esperado `NUM_INT`. |
| `.asciiz 10` | Linha 19, coluna 9: encontrado `NUM_INT("10")`; esperado `STRING`. |
| `.ascii "a", "b"` | Linha 20, coluna 11: encontrado `SMB_COM(",")`; esperado fim de linha (EOL). |
| `.space "x"` | Linha 21, coluna 8: encontrado `STRING("x")`; esperado `NUM_INT`. |
| `.globl` | Linha 22, coluna 7: encontrado fim de linha (EOL); esperado `ID`. |

As linhas 16 (`fim: nop`) e 23 (`ok: .word 1`), corretas, são reconhecidas sem erro.

**`E05_pontuacao`**

| Linha do `.pre` | Diagnóstico |
|---|---|
| `.data extra` | Linha 1, coluna 7: encontrado `ID("extra")`; esperado fim de linha (EOL). |
| `lista: .word 10,` | Linha 2, coluna 17: encontrado fim de linha (EOL); esperado `NUM_INT`. |
| `.word 10 20` | Linha 3, coluna 10: encontrado `NUM_INT("20")`; esperado fim de linha (EOL). |
| `.word , 10` | Linha 4, coluna 7: encontrado `SMB_COM(",")`; esperado `NUM_INT`. |
| `add $t0 $t1, $t2` | Linha 6, coluna 9: encontrado `REG("$t1")`; esperado `SMB_COM(",")`. |
| `add $t0, $t1 $t2` | Linha 7, coluna 14: encontrado `REG("$t2")`; esperado `SMB_COM(",")`. |
| `main li $t0, 1` | Linha 8, coluna 6: encontrado `INS_LI("li")`; esperado `SMB_COL(":")`. |
| `lw $t0, 4 $sp` | Linha 9, coluna 11: encontrado `REG("$sp")`; esperado `SMB_OPA("(")`. |
| `lw $t0, 4($sp` | Linha 10, coluna 14: encontrado fim de linha (EOL); esperado `SMB_CPA(")")`. |
| `lw $t0, 4 $sp)` | Linha 11, coluna 11: encontrado `REG("$sp")`; esperado `SMB_OPA("(")`. |
| `sw $t0, ($sp)` | Linha 12, coluna 9: encontrado `SMB_OPA("(")`; esperado endereço (ID ou NUM_INT). |
| `li $t0 10` | Linha 13, coluna 8: encontrado `NUM_INT("10")`; esperado `SMB_COM(",")`. |
| `li $t0,` | Linha 14, coluna 8: encontrado fim de linha (EOL); esperado `NUM_INT`. |

A linha 15 (`fim: nop`) é reconhecida sem erro. O diagnóstico de `add $t0 $t1, $t2` coincide com o exemplo do enunciado.

### 8.4. Arquivos DOT dos programas válidos

- `testes/sintatico/saidas/V01_exemplo_sem_quebra_final.dot`
- `testes/sintatico/saidas/V02_completo.dot`
- `testes/sintatico/saidas/V03_alternancia_e_limites.dot`

### 8.5. Testes da etapa léxica

Os nove programas da etapa léxica (`testes/entradas/T01` a `T09`) foram executados com a versão atual, e suas saídas estão em `testes/noveTestesExtras/`. Os arquivos `.pre`, `.lex`, `.ts` e os erros léxicos são idênticos aos da entrega anterior.

- `T07`, `T08` e `T09` contêm erros léxicos: a análise sintática não é executada.
- `T01`, `T02`, `T04`, `T05` e `T06` são sintaticamente válidos.
- `T03_correto_memoria` é lexicalmente correto, mas sintaticamente inválido: usa `lw $t5, ($t1)`, e a gramática desta etapa exige o deslocamento em `<endereco> ::= ID | NUM_INT "(" REG ")"`.

### 8.6. Validação da execução

Além da comparação entre o resultado esperado e o obtido em cada caso, foram feitas as seguintes verificações sobre a versão entregue:

| Verificação | Resultado |
|---|---|
| Compilação com o comando do enunciado | Sem erros. |
| Compilação com `-std=c99 -Wall -Wextra -pedantic -Wshadow -Wconversion` | Nenhum aviso. |
| Reexecução dos 8 casos sintáticos em pasta separada | Mensagem, código de encerramento e os 6 arquivos de saída idênticos aos guardados em `testes/sintatico/saidas/`. |
| Reexecução dos 9 casos léxicos (`T01` a `T09`) | Saídas idênticas às guardadas; `.pre`, `.lex`, `.ts` e erros léxicos idênticos aos da entrega da etapa léxica. |
| Execução dos 20 programas `Exemplo01` a `Exemplo20` | 19 sintaticamente válidos; `Exemplo05` tem erro léxico (`$t99`) e a análise sintática não é executada. |
| Estrutura dos 27 arquivos `.dot` gerados | Em todos: raiz `<programa>`, identificadores exclusivos, um único pai por nó, número de arestas igual ao de nós menos um, nenhum não terminal sem filhos. |
| Folhas das árvores | Em todos os 27: a sequência de folhas, sem os `EOL`, é igual à sequência de tokens do `.lex` em categoria, lexema e ordem; há um `EOL` para cada linha com tokens. |
| Programa inválido ou com erro léxico | `.dot` com 0 bytes, inclusive quando o mesmo prefixo já tinha uma árvore de uma execução anterior. |
| Uso incorreto (argumentos a mais ou a menos) | Mensagem de uso e código 2. |
| Arquivo de entrada inexistente ou saída que não pode ser criada | Mensagem de erro e código 2. |
| `.lex` sem `TK_EOF` ou com linha malformada | `TK_EOF` acrescentado internamente; linha malformada faz `AnaliseSintatica` devolver `-1`. |
| Entrada com quebras de linha `\r\n` e arquivo só com comentários | Válido no primeiro caso; rejeitado no segundo, com erro em `TK_EOF`. |

A renderização das árvores pelo Graphviz não pôde ser verificada na máquina de desenvolvimento, que não tem o programa `dot` instalado.

## 9. Limites da etapa

O analisador verifica apenas a estrutura. Não são verificados, por pertencerem à análise semântica ou à montagem: a existência dos rótulos referenciados, a duplicidade de declarações, os limites numéricos dos operandos, as quantidades de deslocamento de `sll` e `srl`, os valores de `.space` e `.align` e os códigos de serviço de `syscall`.
