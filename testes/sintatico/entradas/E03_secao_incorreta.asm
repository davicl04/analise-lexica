# E03 - Construcoes na secao errada (com recuperacao entre as linhas)
.data
valor:  .word   10
        li      $t0, 10         # instrucao em .data
rot:    add     $t0, $t1, $t2   # instrucao apos rotulo em .data
outro:  .byte   1               # linha correta apos os erros
.text
main:   li      $t0, 1          # linha correta
        .word   10              # diretiva de dados em .text
dado:   .asciiz "texto"         # diretiva de dados apos rotulo em .text
        .space  4
        syscall                 # linha correta apos os erros
