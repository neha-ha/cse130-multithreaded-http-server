/***
* Neha Abbas
* neabbas
* 2023 Fall CSE101 PA4
* List.c
* Implementation of List ADT with generic pointer filename
***/

#include "linkedlist.h"

//non-exported struct: means it is not mentioned in List.h, so it is effectively private
//the actual Node in our doubly-linked list
typedef struct NodeObj NodeObj;

//Node is a pointer to NodeObj
//also "private"
typedef NodeObj *Node;

//now actually defining the NodeObj struct
//we do this after we create the Node type so we can use it in the definition
struct NodeObj {
    char filename[69]; //generic pointer
    void *rwlock;
    Node next;
    Node prev;
};

//constructor for a Node
//because it is a pointer, it requires memory allocation
Node create_node(char *file, void *rwl) {
    Node node = calloc(1, sizeof(NodeObj));

    strncpy(node->filename, file, 68);
    node->filename[68] = '\0'; // null terminate

    node->rwlock = rwl;
    node->next = NULL;

    return node;
}

//and deallocation
//destructor:
void node_free(Node *n) {
    if (n != NULL && *n != NULL) {
        free(*n);
        free((*n)->rwlock);
        *n = NULL;
    }
}

//now a private struct for our list
typedef struct ListObj {
    Node front;
    Node back;

    //points to the current element
    Node cursor;
    //index of cursor (current) element
    int index;

    //length of list
    int length;

} ListObj;

//and a struct for a pointer to it
typedef struct ListObj *List;

//constructor
//creates and returns an empty list
List newList() {
    //using calloc instead of malloc because it initializes the memory
    //was getting valgrind uninitialized memory error and this fixed it
    List list = calloc(1, sizeof(ListObj));

    list->front = NULL;
    list->back = NULL;

    list->cursor = NULL;

    list->length = 0;
    list->index = -1;

    return list;
}

//destructor
void freeList(List *l) {
    if (l != NULL && *l != NULL) {

        clear(*l);

        //(*l)->cursor = NULL;

        free(*l);
        *l = NULL;
    }
}

//access functions

//returns # of elements in L
int length(List L) {
    return L->length;
}

int indexx(List L) {
    return L->index;
}

void *front(List L) {
    if (length(L) == 0) {
        fprintf(stderr, "empty list\n");
    }
    return L->front->filename;
}

void *back(List L) {
    if (length(L) == 0) {
        fprintf(stderr, "empty list\n");
    }
    return L->back->filename;
}

char *getfilename(List L) {
    return L->cursor->filename;
}

void *getlock(List L) {
    return L->cursor->rwlock;
}

//manipulation procedures

//resets L to og empty state
void clear(List L) {

    while (length(L) > 0) {
        deleteFront(L);
    }

    node_free(&(L)->cursor);

    L->length = 0;
    L->index = -1;
}

//if L is not empty: sets cursor under (to) first element
//else nothing
void moveFront(List L) {
    if (L->length > 0) {
        L->cursor = L->front;
        L->index = 0;
    }
}

//if L is not empty: sets cursor under (to) back element
//else nothing
void moveBack(List L) {
    if (L->length > 0) {
        L->cursor = L->back;
        L->index = L->length - 1;
    }
}

//if cursor is defined, move cursor back
//if cursor exits bounds, make it undefined
//if cursor is already undefined do nothing
void movePrev(List L) {
    //if cursor not undefined
    if (L->cursor != NULL) {

        //set cursor to previous node
        L->cursor = L->cursor->prev;
        //change index accordingly
        L->index = L->index - 1;

        //check if it went out of bounds
        if (L->index < 0) {
            //set cursor to null
            L->cursor = NULL;
        }
    }
}

//if cursor is defined, move cursor forwards
//if cursor exits bounds, make it undefined
//if cursor is already undefined do nothing
void moveNext(List L) {
    //if cursor not undefined
    if (L->cursor != NULL) {

        //set cursor to next node
        L->cursor = L->cursor->next;
        //change index accordingly
        L->index = L->index + 1;

        //check if it went out of bounds
        if (L->cursor == NULL) {
            //cursor is already null, so we just need to change index
            L->index = -1;
        }
    }
}

//insert element into L after back element
void append(List L, char *fn, void *rwl) {

    //create new node
    Node n = create_node(fn, rwl);

    //if list is empty
    if (L->length == 0) {
        //set front and back to it
        L->front = n;
        L->back = n;
    } else {
        //set back to the new element & rewire
        n->prev = L->back;
        L->back->next = n;
        L->back = n;
    }

    //increase length
    L->length++;
}

//delete front element (precondition: length > 0)
void deleteFront(List L) {

    if (L == NULL) {
        fprintf(stderr, "list is null\n");
        exit(1);
    }
    if (L->length <= 0) {
        fprintf(stderr, "list is empty\n");
        exit(1);
    }
    if (L->length == 1) {
        node_free(&(L->front));
        //nothing to rewire
        L->front = NULL;
        L->cursor = NULL;
        L->back = NULL;
        L->length = 0;
        L->index = -1;
    } else {
        //make cursor undefined if it was at the front
        if (L->cursor == L->front) {
            L->cursor = NULL;
            L->index = -1;
        }

        L->front = L->front->next;
        node_free(&(L->front->prev));
        L->front->prev = NULL;
        L->length -= 1;

        //change cursor index to reflect
        if (L->index != -1) {
            L->index -= 1;
        }
    }
}
