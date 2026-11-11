# E04 - Operandos ausentes, excedentes, fora de ordem ou de categoria inadequada
.text
main:
        add     $t0, $t1            # falta um registrador
        li      $t0, mensagem       # exige um inteiro
        syscall $t0                 # nao admite operandos
        add     $t0, $t1, $t2, $t3  # operando excedente
        addi    $t0, 5, $t1         # fora de ordem
        la      $a0, 10             # exige identificador
        jr      fim                 # exige registrador
        j       $ra                 # exige identificador
        beq     $t0, $t1, 4         # exige identificador
        move    $t0                 # falta o segundo registrador
        lw      $t0, $t1            # endereco invalido
        li      $t0, 1 nop          # tokens adicionais apos construcao completa
        mflo
fim:    nop                         # linha correta apos os erros
.data
        .word                       # falta a lista de inteiros
        .asciiz 10                  # exige string
        .ascii  "a", "b"            # lista de strings nao e admitida
        .space  "x"                 # exige inteiro
        .globl                      # falta o identificador
ok:     .word   1                   # linha correta apos os erros
