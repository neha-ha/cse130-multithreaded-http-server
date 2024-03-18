/***
* Neha Abbas
* neabbas
* 2023 Fall CSE101 PA4 
* List.h
* Header file for List ADT with generic pointers
***/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef LIST
#define LIST

typedef struct ListObj *List;

List newList(void);
void freeList(List *pL);

int length(List L);
int indexx(List L);
void *front(List L);
void *back(List L);
char *getfilename(List L);
void *getlock(List L);

void clear(List L);
void set(List L, void *x);
void moveFront(List L);
void moveBack(List L);
void movePrev(List L);
void moveNext(List L);
void append(List L, char *fn, void *rwl);
void deleteFront(List L);
void delete (List L);

#endif
