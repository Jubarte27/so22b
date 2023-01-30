#include "macros.h"

#ifndef CARREGA_PROGRAMA_H

#define prog(codigo) (programa_cria(codigo, len(codigo)))

#define STRINGIFY(X) STRINGIFY2(X)
#define STRINGIFY2(X) #X

#define ARQUIVO_MAQ(NOME) STRINGIFY(NOME.maq)

#endif

#if defined(VAR)
int VAR[] = {
        #include ARQUIVO_MAQ(VAR)
    };
    #if defined(PROGRAMAS) && defined(I)
        (PROGRAMAS)[I++] = prog(VAR);
    #endif

    #undef MAQ
    #undef VAR
#endif