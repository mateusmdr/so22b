CC = gcc
CFLAGS = -Wall -Werror
LDLIBS = -lcurses

OBJS = exec.o cpu_estado.o es.o mem.o rel.o term.o instr.o err.o \
	tela.o contr.o proc.o so.o teste.o rand.o
OBJS_MONT = instr.o err.o montador.o
PROGRAMAS = benchmark_cpu.maq benchmark_es.maq p1.maq p2.maq \
	grande_cpu_0.maq grande_cpu_1.maq peq_cpu_2.maq peq_cpu_3.maq \
	grande_es_0.maq grande_es_1.maq peq_es_2.maq peq_es_3.maq
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
	rm ${OBJS} ${OBJS_MONT} ${TARGETS} ${MAQS}
