# E05 - Virgulas, dois-pontos e parenteses ausentes; EOL ausente apos a secao
.data extra
lista:  .word   10,                 # lista terminada por virgula
        .word   10 20               # falta a virgula
        .word   , 10                # virgula antes do primeiro inteiro
.text
        add     $t0 $t1, $t2        # falta a primeira virgula
        add     $t0, $t1 $t2        # falta a segunda virgula
main    li      $t0, 1              # falta o dois-pontos do rotulo
        lw      $t0, 4 $sp          # faltam os parenteses
        lw      $t0, 4($sp          # falta fechar o parentese
        lw      $t0, 4 $sp)         # falta abrir o parentese
        sw      $t0, ($sp)          # falta o deslocamento
        li      $t0 10              # falta a virgula
        li      $t0,                # falta o inteiro
fim:    nop                         # linha correta apos os erros
