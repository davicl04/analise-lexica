# T07 - Erros: caracteres invalidos e registradores invalidos
.text
main:
        li      $t0, 10
        add     $t1, $t0, @
        li      $t10, 5
        move    $32, $x1
        add     $t2, $, $t0
        li      $t3, % 2
        sub     $t4, $t0, - 5
