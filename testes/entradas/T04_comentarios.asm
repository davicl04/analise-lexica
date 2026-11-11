# T04 - Comentarios em diversas posicoes
# linha inteira de comentario

.data                   # comentario apos diretiva
msg:  .asciiz "Valor # nao e comentario"   # comentario apos string
        # comentario indentado
.text
main:   # comentario apos rotulo
        li $t0, 1       # comentario apos instrucao
#comentario colado ao inicio da linha
        syscall#comentario colado a instrucao
