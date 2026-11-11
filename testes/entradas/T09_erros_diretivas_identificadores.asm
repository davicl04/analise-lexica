# T09 - Erros: diretivas e identificadores malformados
.dados
. word 5
.data
soma-total: .word 1
inicio@:    .word 2
2valor:     .word 3
valido:     .word 4
.text
main:
        la      $t0, valido
