CC = gcc
CFLAGS = -Wall -Werror
LDLIBS = -lcurses

OBJS = exec.o cpu_estado.o es.o mem.o rel.o term.o instr.o err.o \
			 tela.o contr.o so.o teste.o processo.o vetor.o random.o \
			 escalonador_circular.o
OBJS_MONT = instr.o err.o montador.o
MAQS = init.maq peq_cpu.maq peq_es.maq grande_cpu.maq grande_es.maq p1.maq
TARGETS = teste montador

all: ${TARGETS}

debug: CFLAGS += -DDEBUG -g
debug: ${TARGETS}

# para gerar o montador, precisa de todos os .o do montador
montador: ${OBJS_MONT}

# para gerar o programa de teste, precisa de todos os .o)
teste: ${OBJS}

# para gerar so.o, precisa, além do so.c, dos arquivos .maq
so.o: so.c ${MAQS}

# para transformar um .asm em .maq, precisamos do montador
%.maq: %.asm montador
	./montador $*.asm > $*.maq

clean:
	rm ${OBJS} ${OBJS_MONT} ${TARGETS} ${MAQS}
