CC = gcc
CFLAGS = -Wall -Werror -g3
LDLIBS = -lcurses

OBJS = exec.o cpu_estado.o es.o mem.o rel.o term.o instr.o err.o \
	tela.o contr.o proc.o so.o teste.o rand.o
OBJS_MONT = instr.o err.o montador.o
PROGRAMAS = benchmark_full.maq benchmark_cpu.maq benchmark_es.maq p1.maq p2.maq \
	grande_es_t0.maq grande_es_t1.maq peq_es_t2.maq peq_es_t3.maq \
	grande_cpu_t4.maq grande_cpu_t5.maq peq_cpu_t6.maq peq_cpu_t7.maq \
	
TARGETS = teste montador
MAQS=$(addprefix programas/,$(PROGRAMAS))

all: ${TARGETS}
# para gerar o montador, precisa de todos os .o do montador
montador: ${OBJS_MONT}

# para gerar o programa de teste, precisa de todos os .o)
teste: ${OBJS}

# para gerar proc.o, precisa, além do proc.c, dos arquivos .maq
proc.o: proc.c ${MAQS}

# para transformar um .asm em .maq, precisamos do montador
%.maq: %.asm montador
	./montador $*.asm > $*.maq

clean:
	rm ${OBJS} montador.o ${TARGETS} ${MAQS}
