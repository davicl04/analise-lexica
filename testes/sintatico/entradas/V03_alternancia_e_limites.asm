# V03 - Alternancia de secoes, secoes vazias e limites entre sintaxe e semantica

.text
.data
.text
inicio:
        j       nao_existe      # rotulo inexistente: nao e erro sintatico
.data
x:      .word   1
x:      .word   2               # rotulo duplicado: nao e erro sintatico
        .space  -5              # valor semanticamente invalido
        .align  99
.text
inicio: sll     $t0, $t0, 500   # deslocamento fora do limite
        li      $t0, 0xFFFFFFFFFF
        syscall                 # sem preparar $v0
.data
