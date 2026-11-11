# T01 - Programa lexicamente correto: soma de dois valores e impressao
.data
msg:      .asciiz "Resultado: "
.text
.globl main
main:
        li      $t0, 5          # primeiro valor
        li      $t1, 10         # segundo valor
        add     $t2, $t0, $t1   # $t2 = 15
        la      $a0, msg
        li      $v0, 4          # imprimir string
        syscall
        move    $a0, $t2
        li      $v0, 1          # imprimir inteiro
        syscall
        li      $v0, 10         # encerrar
        syscall
