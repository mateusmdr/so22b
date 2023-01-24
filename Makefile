CC = gcc
CFLAGS = -Wall -Werror
LDLIBS = -lcurses

OBJS = exec.o cpu_estado.o es.o mem.o rel.o term.o instr.o err.o \
	tela.o contr.o proc.o so.o teste.o
OBJS_MONT = instr.o err.o montador.o
MAQS = ex1.maq ex2.maq ex3.maq ex4.maq ex5.maq init.maq p1.maq p2.maq grande_cpu.maq grande_es.maq peq_cpu.maq peq_es.maq
TARGETS = teste montador

all: ${TARGETS}
# para gerar o montador, precisa de todos os .o do montador
montador: ${OBJS_MONT}

# para gerar o programa de teste, precisa de todos os .o)
teste: ${OBJS}

# para gerar so.o, precisa, além do so.c, dos arquivos .maq
proc.o: proc.c ${MAQS}

# para transformar um .asm em .maq, precisamos do montador
%.maq: %.asm montador
	./montador $*.asm > $*.maq

clean:
	rm ${OBJS} ${OBJS_MONT} ${TARGETS} ${MAQS}
