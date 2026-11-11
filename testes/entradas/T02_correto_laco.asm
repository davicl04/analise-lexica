# T02 - Programa lexicamente correto: laco com desvios e referencia antecipada
.text
main:
        li      $t0, 0
        li      $t1, 10
        j       teste           # referencia antecipada ao rotulo
laco:
        addi    $t0, $t0, 1
        mul     $t2, $t0, $t0
        slt     $t3, $t0, $t1
teste:
        blt     $t0, $t1, laco
        bne     $t0, $t1, laco
        jal     fim
fim:
        jr      $ra
        nop
