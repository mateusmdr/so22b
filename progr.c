int progr0[] = {
#include "programas/benchmark_cpu.maq"
};

int progr1[] = {
#include "programas/benchmark_es.maq"
};

int progr2[] = {
#include "programas/grande_cpu_0.maq"
};

int progr3[] = {
#include "programas/grande_cpu_1.maq"
};

int progr4[] = {
#include "programas/peq_cpu_2.maq"
};

int progr5[] = {
#include "programas/peq_cpu_3.maq"
};

int progr6[] = {
#include "programas/grande_es_0.maq"
};

int progr7[] = {
#include "programas/grande_es_1.maq"
};

int progr8[] = {
#include "programas/peq_es_2.maq"
};

int progr9[] = {
#include "programas/peq_es_3.maq"
};

// programas disponíveis
int *progrs[] = {
    progr1,
    progr2,
    progr3,
    progr4,
    progr5,
    progr6,
    progr7,
    progr8,
    progr9
};
int progrs_size[] = {
    sizeof(progr0),
    sizeof(progr1),
    sizeof(progr2),
    sizeof(progr3),
    sizeof(progr4),
    sizeof(progr5),
    sizeof(progr6),
    sizeof(progr7),
    sizeof(progr8),
    sizeof(progr9),
};