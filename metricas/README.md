
# Benchmarks - Memória virtual

#### Tamanho da memória principal
O *benchmark_full* precisa de um pouco mais de 1600 de memória para executar, portanto, para a configuração de folga será usado **MEM_TAM** = 800, e para pouca memória, **MEM_TAM** = 300.
Em ambos os casos o tamanho dos quadros será de 50, para simular um sistema real onde o tamanho dos quadros é definido pelo *hardware*.

#### Algoritmos de substituição de páginas
- Por simplicidade, implementei um algoritmo que escolhe aleatoriamente um quadro para ser substituído (podendo ser, inclusive, a página atual do processo), portanto se espera que este tenha um péssimo desempenho (vai fazer muitas cópias de memória).
- Também implementei o **FIFO**, que retira o quadro mais antigo da memória.

Ambos os algoritmos só executam quando toda a memória está ocupada. Quando há memória disponível, o quadro escolhido é aquele com o menor índice.

#### Configurações padrão
Eu queria fazer o *benchmark* de todos os casos, variando os programas, o escalonador de processos, o quantum, o algoritmo de substituição de páginas e a quantidade de memória, mas sem automatizar o processo demoraria demais (totalizando 48 configurações diferentes).
Portanto, vou testar só a parte de memória virtual com o escalonador **ROUND_ROBIN** e **MAX_QUANTUM** = 2.

#### Novas métricas:
- Número de falhas de página por processo
- Total de falhas de página registradas pelo SO

#### Análise:
Como esperado, o algoritmo aleatório causou mais falhas de página que o **FIFO**, já que frequentemente ele descartava páginas que estavam sendo utilizadas, no entanto, achei que a diferença fosse ser mais expressiva.

Nas situações de pouca memória, o número de falha de páginas aumentou absurdamente (foi de aproximadamente 90 para +1300!). Isso foi acompanhado de um leve aumento no tempo de execução, devido às movimentações de memória causadas pela troca de páginas, que foi ainda pior no algoritmo aleatório.

Talvez se o quantum do escalonador fosse maior, as discrepâncias entre os dois algorimos fosse mais evidente, já que a preempção também força algumas falhas de página em situações de pouca memória.

Em todos os casos, a **CPU** ficou ativa 100% do tempo. Isto se deve ao fato de as movimentações de memória serem tão demoradas que estas dão tempo de o dispositivo de **E/S** ficar pronto enquanto outros processos são executados, além de que os programas intensivos de **CPU** são muito mais demorados que os de **E/S**.