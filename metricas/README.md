#Benchmarks

##As métricas foram divididas da seguinte forma:
- **FULL** (dois processos de cada programa fornecido)
- **CPU** (dois processos de peq_cpu e dois processos de grande_cpu)
- **ES** (dois processos de peq_es e dois processos de grande_es)
Cada uma contendo as quatro configurações citadas na descrição do trabalho.
Um quantum "pequeno" foi considerado como sendo 2 interrupções de relógio, já um "grande", 16.

###Além das métricas pedidas, foram adicionadas:
- Tempo real de execução do sistema (em segundos)
- Número de *SISOPS* (para contrastar com o número de interrupções)

####Os resultados dos benchmarks estão disponíveis organizados em pastas, com as métricas do SO e de cada processo. (auto-evidente)
