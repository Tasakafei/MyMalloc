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

/*********************/
/***    GETTERS    ***/
/*********************/
/*
 * Récupère le block du pointeur
 */
Header getHeader(void *ptr) {
    char *tmp;
    tmp = ptr;
    return (ptr = tmp -= HEADER_SIZE);
}

/*
 * Recupère le pointeur sur le block
 */
int getPtr(Header block) {
    int adr = block;
    adr += HEADER_SIZE;
    return adr;
}

/***********************/
/***    FONCTIONS    ***/
/***********************/


/**
 * On étend de size à partir de last
 */
Header extendHeap(Header last, size_t size) {
    nb_sbrk;
    Header newBlock;
    newBlock = sbrk(0);
    // si on a plus de place
    if (sbrk(HEADER_SIZE + size) == (void*)-1) return NULL;
    //sinon
    newBlock->info.size = size;
    newBlock->info.next = NULL;

    if (last)
        last->info.next = newBlock;
    else
        base = newBlock;
    
    return newBlock;
}

/*
 * Recherche un bloc libre d'une taille minimum size
 */
 
Header findBlock(Header *last, size_t size) {
    Header iBlock = base;
    while (iBlock && (iBlock->info.size < size)) {
        *last = iBlock;
        iBlock = iBlock->info.next;
    }

    return iBlock;
}

/**
 * Coupe le bloc en 2 afin de créer un bloc libre
 */
void splitBlock(Header block, size_t size) {
    // Création du bloc libre
    Header secondBlock = getPtr(block) + size;
    secondBlock->info.size = (block->info.size - HEADER_SIZE - size);
    secondBlock->info.next = block->info.next;

    block->info.size = size;
    block->info.next = secondBlock;
}

/**
 * Supprime le bloc de la liste des blocs libres
 */
void removeBlock(Header block) {
    if (block == base) {
        base = (block->info.next) ? block->info.next : NULL;
    } else if (base) {
        Header iBlock = base;
        while (iBlock->info.next && iBlock->info.next != block)
            iBlock = iBlock->info.next;
        iBlock->info.next = (iBlock->info.next) ? iBlock->info.next->info.next : NULL;
    }
}

/**
 * Fusionne le bloc en paramètre avec le suivant si possible
 * @param block Bloc courant
 */
void merge(Header block) {
    Header nextBlock = block->info.next;
    if (nextBlock) {
        // printf("\nBefore merge %d, size=%d, next=%d", block, block->info.size, block->info.next);
        // printf("\nBefore merge %d, size=%d, next=%d\n", nextBlock, nextBlock->info.size, nextBlock->info.next);
        if ((getPtr(block) + block->info.size) == nextBlock) {
            block->info.size += (HEADER_SIZE + nextBlock->info.size);
            block->info.next = nextBlock->info.next;

            // "Supprime" le block fusionné
            nextBlock->info.size = 0;
            nextBlock->info.next = NULL;
        }
        // printf("After merge %d, size=%d, next=%d\n", block, block->info.size, block->info.next);
        // printf("After merge %d, size=%d, next=%d\n\n", nextBlock, nextBlock->info.size, nextBlock->info.next);
    }
}
////////////////

/**
 * A first fit malloc
 * @see malloc(3)
 *
 * 1. Recherche un bloc libre assez grand
 * 2. Si trouvé, split le bloc et renvoie juste un bloc de taille size
 * 3. Si non trouvé, sbrk la mémoire
 */
void *mymalloc(size_t size) {
    nb_alloc += 1;
    if (!base) { 
        extendHeap(NULL, FIRST_BLOCK);
    }
    Header new, last;
    size_t alignedSize = align8(size);
    if (base) {
        new = findBlock(&last, alignedSize);
        if (!new) {
            // On a pas de bon bloc, on sbrk
            new = extendHeap(last, FIRST_BLOCK);
            if (!new) return NULL;
        }      
        if ((new->info.size - alignedSize) >= (HEADER_SIZE + MIN_BLOCK_SIZE))
        	splitBlock(new, alignedSize);
    }
    removeBlock(new);
    return getPtr(new);
}

/**
 * @see free(3)
 *
 * 1. Remet le bloc dans la liste des blocs libres
 * 2. S'il est contigu => fusion
 */
void myfree(void *ptr) {
    nb_dealloc += 1;
    Header ptrBlock = getHeader(ptr);
    if (!base) {
        base = ptrBlock;
    } else {
        if (ptrBlock < base) {
            ptrBlock->info.next = base;
            base = ptrBlock;
            // Vérifie s'il est contigu
            merge(base); // Avec le suivant
        } else {
		    Header iBlock = base;
		    while (iBlock->info.next && iBlock->info.next < ptrBlock) {
		        iBlock = iBlock->info.next;
		    }

		    if (ptrBlock->info.size != 0) {
		        // Remet le bloc dans la liste
		        ptrBlock->info.next = iBlock->info.next;
		        iBlock->info.next = ptrBlock;
		    }
		    // Vérifie s'il est contigu
		    merge(ptrBlock); // Avec le suivant
		    merge(iBlock); // Avec le précédent
        }
    }
}

/**
 * @see calloc(3)
 *
 * 1. Alloue un nouveau bloc de nb*size
 * 2. init à 0
 * @TODO le init à 0, casse la size..
 */
void *mycalloc(size_t nmemb, size_t size) {
    nb_alloc += 1;
    if (nmemb == 0 || 0 == size) return NULL;

    size_t i, alignedSize;
    alignedSize = align8(nmemb*size);
    void *newData = mymalloc(alignedSize);
    if (newData)
        bzero(newData, alignedSize);

    // Header newBlock = getHeader(newData);
    // printf("mycalloc %d, size=%d, next=%d, data=%d\n", newBlock, newBlock->info.size, newBlock->info.next, *(newBlock+HEADER_SIZE+2));
    return newData;
}

/**
 * @see realloc(3)
 *
 * 1. Alloue un nouveau bloc de size
 * 2. Copie les données de l'ancien vers les nouveau bloc
 * 3. Free l'ancien bloc
 */
void *myrealloc(void *ptr, size_t size) {
    nb_alloc += 1;
    size_t alignedSize = align8(size);

    if (ptr) {
        Header oldBlock = getHeader(ptr);
        if (alignedSize > oldBlock->info.size) { // Si le ptr != NULL et nouvelle taille > oldSize => 
        		                                   // malloc et PAS d'initialisation
            unsigned int oldSize = oldBlock->info.size;
            myfree(ptr);
            void *newData = mymalloc(alignedSize);
            memcpy(newData, ptr, oldSize);

            return newData;
        } else { // Taille voulu <= à la précédente
            return ptr;
        }
        if (alignedSize == 0) {
            myfree(ptr);
            return NULL;
        }
    } else {
        return mymalloc(alignedSize); // Si ptr == null => malloc
    }
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

