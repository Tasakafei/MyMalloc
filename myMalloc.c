#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include "my-malloc.h"

/************************/
/***    CONSTANTES    ***/
/************************/
#define MOST_RESTRICTING_TYPE double // Type le plus contraignant
#define HEADER_SIZE sizeof(Header)

#define MIN_BLOCK_SIZE 8
#define BLOCK 400


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
    printf("\n++DEBUG++\n");
    // on print tout !
    for (Header iBlock = base; iBlock; iBlock = iBlock->info.next)
        printf("Block %p (%d) => next %p\n", iBlock, iBlock->info.size, iBlock->info.next);
    printf("--DEBUG--\n\n");
}

/*********************/
/***    GETTERS    ***/
/*********************/

// Récupération du block du pointeur
Header getHeader(void *ptr) {
    char *tmp;
    tmp = ptr;
    return (ptr = tmp -= HEADER_SIZE);
}

size_t getAlign(size_t size) {
	return (((size-1)>>3)<<3)+8;
}

// Récupération d'un pointeur sur le block
int getPtr(Header block) {
    int adr = block; // ICI
    adr += HEADER_SIZE;
    return adr;
}

/***********************/
/***    FONCTIONS    ***/
/***********************/


// Ajoute d'un espace mémoire de taille size à partir de last.
// Retourne NULL si erreur sinon retourne le Header du nouveau block.
Header extendMem(Header last, size_t size) {
    nb_sbrk++;
    Header newBlock;
    // récupération du dernier break
    newBlock = sbrk(0);                             // On peut peut etre remplacer ces trois lignes par :
    // si le sbrk n'a pas marché, on retourne NULL  // if ((newBlock = sbrk(HEADER_SIZE + size)) == (void*)-1)
    if (sbrk(HEADER_SIZE + size) == (void*)-1)      //      return NULL;
        return NULL;                                // Mais je suis pas sur..
    // On met les infos du newBlock à jour
    newBlock->info.size = size;
    newBlock->info.next = NULL;
    // Si last n'est pas NULL alors on le lie à ce nouveau bloc
    if (last)
        last->info.next = newBlock; // Y faut pas du coup faire un merge avec last et newBlock ?
    // Sinon ce nouveau block deviens la base
    else
        base = newBlock;
    return newBlock;
}


 // Recherche d'un block libre ayant une taille supérieure ou égale à size
Header findBlock(Header *last, size_t size) {
    Header Block;
    // On parcours tous les blocs à la recherche d'un bloc de taille suffisante
    for (Block = base; Block && (Block->info.size < size); Block = Block->info.next)
        *last = Block;
    return Block;
}

 // Découpage d'un bloc en deux pour en crée un nouveau bloc libre de taille size
void splitBlock(Header block, size_t size) {
    // Création du Header du nouveau bloc
    Header secondBlock = getPtr(block) + size;
    // Ajout des informations dans le Header de ce nouveau bloc
    secondBlock->info.size = (block->info.size - HEADER_SIZE - size);
    secondBlock->info.next = block->info.next;
    // Mise à jour des informations de l'ancien Header du bloc coupé
    block->info.size = size;
    block->info.next = secondBlock;
}

 // Suppression d'un bloc
void removeBlock(Header block) {
    // Si le bloc est la base, on le remplace par la bloc suivant.
    // Si aucun bloc le succède, on le remplace par NULL
    if (block == base) {
        base = (block->info.next) ? block->info.next : NULL;
    }
    else if (base) {
        Header iBlock = base;
        // On parrcours les blocs afin de trouver le bloc précédant le bloc recherché
        while (iBlock->info.next && iBlock->info.next != block)
            iBlock = iBlock->info.next;
        iBlock->info.next = (iBlock->info.next) ? iBlock->info.next->info.next : NULL;
    }
}

 // Fusion d'un bloc avec son suivant
void merge(Header block) {
    // Récupération du Header du bloc suivant
    Header nextBlock = block->info.next;
    // Si il n'est pas NULL on fusionne les deux blocs
    if (nextBlock) {
        // Test si les deux blocs se suivent, si oui on peut les fusionner sinon non.
        if ((getPtr(block) + block->info.size) == nextBlock) {
            // Mise à jour des informations du premier bloc
            block->info.size += (HEADER_SIZE + nextBlock->info.size); // Rajout au premier la taille du second bloc
            block->info.next = nextBlock->info.next; // Liaison avec le premier et le bloc suivant le deuxième bloc

            // Suppresion du deuxième block
            nextBlock->info.size = 0;    // Y'a vraiment besoin de faire ces deux ligne ?
            nextBlock->info.next = NULL; // Ou pas ? ^^
        }
    }
}

 // Recherche d'un bloc libre de taille suffisante. Si on en trouve un qui convient
 // on le découpe et on lui renvoie un bloc de taille size.
 // Sinon, on rajoute de la mémoire
void *mymalloc(size_t size) { // Peut-etre besoin d'une bloc dans cette fonction
    nb_alloc++;
    // Si il n'y a aucun bloc de libre, on ajoute de la mémoire
    if (base == NULL) { 
        extendMem(NULL, BLOCK);
    }
    Header new, last;
    size_t alignedSize = getAlign(size);
    if (base) {
        // Recherche d'un bloc libre de taille suffisante
        new = findBlock(&last, alignedSize);
        if (!new) {
            // On a pas de bon bloc, on sbrk
            new = extendMem(last, BLOCK);
            if (!new) return NULL;
        }      
        if ((new->info.size - alignedSize) >= (HEADER_SIZE + MIN_BLOCK_SIZE))
        	splitBlock(new, alignedSize);
    }
    removeBlock(new);
    return getPtr(new);
}

 // Remise du bloc dans la liste des blocs libre. Si il est collé au
 // bloc précédent (ou suivant) on les fusionnes
void myfree(void *ptr) {
    nb_dealloc += 1;
    Header ptrBlock = getHeader(ptr);
    // Si la liste est vide, la base deviens ce bloc
    if (!base) {
        base = ptrBlock;
    }
    else {
        // Si ce bloc est avant la base, la base deviens ce bloc
        if (ptrBlock < base) {
            // Rajout des information de ce nouveau bloc
            ptrBlock->info.next = base;
            base = ptrBlock;
            // Fusion du bloc avec le suivant si necessaire
            merge(base);
        } else {
		    Header iBlock = base;
            // Parcours de tous les bloc libre à la recherche
            //du bloc précédant le bloc que l'on veut liberer
		    while (iBlock->info.next && iBlock->info.next < ptrBlock) {
		        iBlock = iBlock->info.next;
		    }

		    if (ptrBlock->info.size != 0) {
		        // Remise du bloc dans la liste des blocs libres
		        ptrBlock->info.next = iBlock->info.next;
		        iBlock->info.next = ptrBlock;
		    }
		    // Fusion de ce nouveau bloc libre avec le suivant et le précédent si necessaire
		    merge(ptrBlock);
		    merge(iBlock);
        }
    }
}

 // Allocation d'un nouveau bloc de taille (nmemb*size), puis initialisation de ce nouveau bloc à 0
void *mycalloc(size_t nmemb, size_t size) {
    nb_alloc++;
    // Test des paramètres
    if (nmemb == 0 || 0 == size) return NULL;

    size_t i, alignedSize;
    alignedSize = getAlign(nmemb*size);
    void *newData = mymalloc(alignedSize);
    // Initialisation du bloc à 0 si différent de NULL
    if (newData)
        bzero(newData, alignedSize);
    return newData;
}

 // Allocation d'un nouveau bloc de taille size, copie de l'ancien bloc dans ce nouveau.
 // Puis on libère l'ancien bloc
void *myrealloc(void *ptr, size_t size) {
    nb_alloc++;
    size_t alignedSize = getAlign(size);
    if (ptr != NULL) {
        Header firstBlock = getHeader(ptr);
        if (alignedSize > firstBlock->info.size) {
            size_t oldSize = firstBlock->info.size;
            myfree(ptr);
            void *newData = mymalloc(alignedSize);
            memcpy(newData, ptr, oldSize);
            return newData;
        } else {
            return ptr;
        }
        if (alignedSize == 0) {
            myfree(ptr);
            return NULL;
        }
    } else {
        return mymalloc(alignedSize);
    }
}

void mymalloc_infos(char *str)
{
     if (str) fprintf(stderr, "**********\n*** %s\n", str);
    fprintf(stderr, "# allocs = %3d - # deallocs = %3d - # sbrk = %3d\n",nb_alloc, nb_dealloc, nb_sbrk);
    if (base)
        for (Header iBlock = base; iBlock; iBlock = iBlock->info.next)
            fprintf(stderr, "\tBlock @ 0x%X (size=\t%d,\t next 0x%X)\n", iBlock, iBlock->info.size / MIN_BLOCK_SIZE, iBlock->info.next);
}

