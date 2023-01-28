#ifndef PROGR_H
#define PROGR_H

#include "progr.h"

int progr0[] = {
#include "programas/benchmark_full.maq"
};

int progr1[] = {
#include "programas/benchmark_es.maq"
};

int progr2[] = {
#include "programas/benchmark_cpu.maq"
};

int progr3[] = {
#include "programas/grande_es_t0.maq"
};

int progr4[] = {
#include "programas/grande_es_t1.maq"
};

int progr5[] = {
#include "programas/peq_es_t2.maq"
};

int progr6[] = {
#include "programas/peq_es_t3.maq"
};

int progr7[] = {
#include "programas/grande_cpu_t4.maq"
};

int progr8[] = {
#include "programas/grande_cpu_t5.maq"
};

int progr9[] = {
#include "programas/peq_cpu_t6.maq"
};

int progr10[] = {
#include "programas/peq_cpu_t7.maq"
};

int progr11[] = {
#include "programas/p1.maq"
};

int progr12[] = {
#include "programas/p2.maq"
};

// programas disponíveis
int* PROGRS[] = {
    progr0,
    progr1,
    progr2,
    progr3,
    progr4,
    progr5,
    progr6,
    progr7,
    progr8,
    progr9,
    progr10,
    progr11,
    progr12
};

// tamanho de cada programa
int PROGRS_SIZE[] = {
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
    sizeof(progr10),
    sizeof(progr11),
    sizeof(progr12)
};

#endif