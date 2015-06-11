#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include "my-malloc.h"

/************************/
/***    CONSTANTES    ***/
/************************/
#define MOST_RESTRICTING_TYPE double // Type le plus contraignant
#define HEADER_SIZE sizeof(union header)

#define MIN_BLOCK_SIZE 8
#define FIRST_BLOCK 800

typedef union header { // Header du bloc
    struct {
        unsigned int size;  // Taille du bloc
        union header *next; // Bloc libre suivant
    } info;
    MOST_RESTRICTING_TYPE dummy; // Ne sert qu'à provoquer un alignement
} *Header;

/**************************/
/***    VAR GLOBALES    ***/
/**************************/
static Header base = NULL;
static int nb_alloc   = 0;
static int nb_dealloc = 0;
static int nb_sbrk    = 0;

/**********************/
/***    DEBUGGER    ***/
/**********************/
void printDebug() {
    printf("\nDEBUT DEBUG\n");
    // on print tout !
    for (Header iBlock = base; iBlock; iBlock = iBlock->info.next)
        printf("Block %d (%d) => next %d\n", iBlock, iBlock->info.size, iBlock->info.next);
    printf("FIN DEBUG\n\n");
}


void mymalloc_infos(char *str)
{
    if (str) fprintf(stderr, "**********\n*** %s\n", str);

    fprintf(stderr, "# allocs = %3d - # deallocs = %3d - # sbrk = %3d\n",
        nb_alloc, nb_dealloc, nb_sbrk);
    /* Ca pourrait être pas mal d'afficher ici les blocs dans la liste libre */
    if (base)
    {
        for (Header iBlock = base; iBlock; iBlock = iBlock->info.next)
        {
            fprintf(stderr, "Block %d (size=%d, next %d)\n", iBlock, iBlock->info.size, iBlock->info.next);
        }
    }
}
