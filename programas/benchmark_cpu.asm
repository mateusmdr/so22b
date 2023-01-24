; benchmark intensivo de cpu
; cria dois processos grande_cpu e dois processos peq_cpu
SO_FIM  define 3
SO_CRIA define 4
        cargi 6
        sisop SO_CRIA
        cargi 7
        sisop SO_CRIA
        cargi 8
        sisop SO_CRIA
        cargi 9
        sisop SO_CRIA
        
        sisop SO_FIM
