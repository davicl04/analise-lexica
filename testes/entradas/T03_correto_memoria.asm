# T03 - Acesso a memoria no formato deslocamento(registrador)
.data
vetor:  .word 10, 20, 30, 40
buffer: .space 16
.text
main:
        la      $t1, vetor
        lw      $t0, 0($t1)
        lw      $t2, 4($t1)
        sw      $t2, 8($t1)
        lb      $t3, 1($t1)
        sb      $t3, 0($sp)
        lh      $t4, 2($t1)
        sh      $t4, -8($fp)
        lw      $t5, ($t1)
