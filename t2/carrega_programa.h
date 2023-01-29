#include "macros.h"

#ifndef CARREGA_PROGRAMA_H

#define prog(codigo) (programa_cria(codigo, len(codigo)))

#endif

#if defined(MAQ) && defined(VAR)
    int VAR[] = {
        #include MAQ
    };
    #if defined(PROGRAMAS) && defined(I)
        (PROGRAMAS)[I++] = prog(VAR);
    #endif

    #undef MAQ
    #undef VAR
#endif