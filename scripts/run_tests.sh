#!/usr/bin/env bash
# Compila o projeto e compara a saída de cada teste com os arquivos esperados.
#
# Uso: scripts/run_tests.sh            (a partir da raiz do repositório)
# Variáveis opcionais: CC (padrão gcc), CFLAGS.
#
# Suítes com saída esperada:
#   testes/entradas/T*.asm         -> testes/noveTestesExtras/
#   testes/sintatico/entradas/*.asm -> testes/sintatico/saidas/
# Os Exemplo*.asm não têm saída esperada: só se verifica que não há falha
# operacional (código de saída 2) nem travamento.

set -u

cd "$(dirname "$0")/.."

CC="${CC:-gcc}"
CFLAGS="${CFLAGS:--std=c99 -Wall -Wextra -pedantic -Werror}"
BIN="build/main"
OUT="build/saidas"
SUFIXOS=(.pre .lex .ts .lex.err .sin.err .dot)

mkdir -p build "$OUT"

echo "==> Compilando: $CC $CFLAGS *.c"
# shellcheck disable=SC2086
if ! $CC $CFLAGS ./*.c -o "$BIN"; then
    echo "FALHA: compilação"
    exit 1
fi

aprovados=0
reprovados=0

# executar <entrada.asm> <prefixo>: devolve o código de saída do programa
executar() {
    timeout 10 "$BIN" "$1" "$2" >/dev/null
}

# suite <dir_entradas> <padrão> <dir_esperado>
suite() {
    local entradas="$1" padrao="$2" esperado="$3"
    local asm nome codigo ok sufixo

    echo "==> Suíte: $entradas/$padrao  (esperado em $esperado)"
    for asm in "$entradas"/$padrao; do
        [ -e "$asm" ] || continue
        nome="$(basename "$asm" .asm)"
        executar "$asm" "$OUT/$nome"
        codigo=$?
        ok=1

        if [ "$codigo" -ne 0 ] && [ "$codigo" -ne 1 ]; then
            echo "  FALHOU  $nome (código de saída $codigo)"
            ok=0
        fi

        for sufixo in "${SUFIXOS[@]}"; do
            if [ ! -f "$esperado/$nome$sufixo" ]; then
                continue
            fi
            if ! diff -u --strip-trailing-cr "$esperado/$nome$sufixo" "$OUT/$nome$sufixo" >"$OUT/$nome$sufixo.diff"; then
                echo "  FALHOU  $nome$sufixo difere do esperado:"
                sed 's/^/          /' "$OUT/$nome$sufixo.diff" | head -40
                ok=0
            else
                rm -f "$OUT/$nome$sufixo.diff"
            fi
        done

        if [ "$ok" -eq 1 ]; then
            echo "  ok      $nome"
            aprovados=$((aprovados + 1))
        else
            reprovados=$((reprovados + 1))
        fi
    done
}

suite testes/entradas "T*.asm" testes/noveTestesExtras
suite testes/sintatico/entradas "*.asm" testes/sintatico/saidas

echo "==> Execução sem saída esperada: testes/entradas/Exemplo*.asm"
for asm in testes/entradas/Exemplo*.asm; do
    [ -e "$asm" ] || continue
    nome="$(basename "$asm" .asm)"
    executar "$asm" "$OUT/$nome"
    codigo=$?
    if [ "$codigo" -eq 0 ] || [ "$codigo" -eq 1 ]; then
        echo "  ok      $nome (código $codigo)"
        aprovados=$((aprovados + 1))
    else
        echo "  FALHOU  $nome (código de saída $codigo)"
        reprovados=$((reprovados + 1))
    fi
done

echo
echo "Resultado: $aprovados aprovados, $reprovados reprovados."
[ "$reprovados" -eq 0 ]
