/*
 * Author: Suki Sahota
 */
#ifndef _MY_LIST_H_
#define _MY_LIST_H_

#include "my_math.h"

typedef struct tagMyListElem {
    void *obj;
    struct tagMyListElem *next;
    struct tagMyListElem *prev;
} MyListElem;

typedef struct tagMyList {
    int num_members;
    MyListElem anchor;

    /* Function pointers */
    int  (*Length)(struct tagMyList *);
    int  (*Empty)(struct tagMyList *);

    int  (*Append)(struct tagMyList *, void*);
    int  (*Prepend)(struct tagMyList *, void*);
    void (*Unlink)(struct tagMyList *, MyListElem*);
    void (*UnlinkAll)(struct tagMyList *);
    int  (*InsertBefore)(struct tagMyList *, void*, MyListElem*);
    int  (*InsertAfter)(struct tagMyList *, void*, MyListElem*);

    MyListElem *(*First)(struct tagMyList *);
    MyListElem *(*Last)(struct tagMyList *);
    MyListElem *(*Next)(struct tagMyList *, MyListElem *cur);
    MyListElem *(*Prev)(struct tagMyList *, MyListElem *cur);

    MyListElem *(*Find)(struct tagMyList *, void *obj);
} MyList;

extern int  MyListLength(MyList*);
extern int  MyListEmpty(MyList*);

extern int  MyListAppend(MyList*, void*);
extern int  MyListPrepend(MyList*, void*);
extern void MyListUnlink(MyList*, MyListElem*);
extern void MyListUnlinkAll(MyList*);
extern int  MyListInsertAfter(MyList*, void*, MyListElem*);
extern int  MyListInsertBefore(MyList*, void*, MyListElem*);

extern MyListElem *MyListFirst(MyList*);
extern MyListElem *MyListLast(MyList*);
extern MyListElem *MyListNext(MyList*, MyListElem*);
extern MyListElem *MyListPrev(MyList*, MyListElem*);

extern MyListElem *MyListFind(MyList*, void*);

extern int MyListInit(MyList*);

#endif /*_MY_LIST_H_*/
