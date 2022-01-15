/*
 * Author: Suki Sahota
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include "my_math.h"

#include "my_list.h"

/* ----------------------- Utility Functions ----------------------- */

int  MyListLength(MyList *list) {
    return list->num_members;
}

int  MyListEmpty(MyList *list) {
    return (list->num_members <= 0);
}

int  MyListAppend(MyList *list, void *obj) {
    MyListElem *tmp_elem = (MyListElem *) malloc(sizeof(MyListElem));
    tmp_elem->obj = obj;
    tmp_elem->next = &(list->anchor);

    if (MyListEmpty(list)) {
        tmp_elem->prev = &(list->anchor);
        list->anchor.next = tmp_elem;
    } else {
        tmp_elem->prev = list->anchor.prev;
        list->anchor.prev->next = tmp_elem;
    }
    list->anchor.prev = tmp_elem;
    ++(list->num_members);
    return TRUE;
}

int  MyListPrepend(MyList *list, void *obj) {
    MyListElem *tmp_elem = (MyListElem *) malloc(sizeof(MyListElem));
    tmp_elem->obj = obj;
    tmp_elem->prev = &(list->anchor);

    if (MyListEmpty(list)) {
        tmp_elem->next = &(list->anchor);
        list->anchor.prev = tmp_elem;
    } else {
        tmp_elem->next = list->anchor.next;
        list->anchor.next->prev = tmp_elem;
    }
    list->anchor.next = tmp_elem;
    ++(list->num_members);
    return TRUE;
}

void MyListUnlink(MyList *list, MyListElem *elem) {
    /*
     * Unlink from object
     * Update elements prev -> next pointers
     * Update elements next -> prev pointers
     * Deallocate memory
     */
    elem->obj = NULL;
    elem->prev->next = elem->next;
    elem->next->prev = elem->prev;
    free(elem);
    --(list->num_members);
}

void MyListUnlinkAll(MyList *list) {
    if (!MyListEmpty(list)) {
        MyListElem *lead = list->anchor.next;

        for (int list_length = MyListLength(list); 
             list_length > 0; --list_length) {
            lead = lead->next;
            lead->prev->obj = NULL;
            free(lead->prev);
        }
    list->num_members = 0;
    }
}

int  MyListInsertAfter(MyList *list, void *obj, MyListElem *elem) {
    MyListElem *tmp_elem = (MyListElem *) malloc(sizeof(MyListElem));
    tmp_elem->obj = obj;
    if (elem == NULL) {
        return MyListAppend(list, obj);
    } else {
        tmp_elem->prev = elem;
        tmp_elem->next = elem->next;
        elem->next->prev = tmp_elem;
        elem->next = tmp_elem;
    }
    ++(list->num_members);
    return TRUE;
}

int  MyListInsertBefore(MyList *list, void *obj, MyListElem *elem) {
    MyListElem *tmp_elem = (MyListElem *) malloc(sizeof(MyListElem));
    tmp_elem->obj = obj;
    if (elem == NULL) {
        return MyListPrepend(list, obj);
    } else {
        tmp_elem->prev = elem->prev;
        tmp_elem->next = elem;
        elem->prev->next = tmp_elem;
        elem->prev = tmp_elem;
    }
    ++(list->num_members);
    return TRUE;
}

MyListElem *MyListFirst(MyList *list) {
    if (MyListEmpty(list)) {
        return NULL;
    } else {
        return list->anchor.next;
    }
}

MyListElem *MyListLast(MyList *list) {
    if (MyListEmpty(list)) {
        return NULL;
    } else {
        return list->anchor.prev;
    }
}

MyListElem *MyListNext(MyList *list, MyListElem *elem) {
    if (elem == MyListLast(list)) {
        return NULL;
    } else {
        return elem->next;
    }
}

MyListElem *MyListPrev(MyList *list, MyListElem *elem) {
    if (elem == MyListFirst(list)) {
        return NULL;
    } else {
        return elem->prev;
    }
}

MyListElem *MyListFind(MyList *list, void *obj) {
    if (MyListEmpty(list)) { return NULL; }

    MyListElem *tmp_elem = list->anchor.next;
    while (tmp_elem->obj != obj && tmp_elem->next != &(list->anchor)) {
        tmp_elem = tmp_elem->next;
    }
    return (tmp_elem->obj == obj) ? tmp_elem : NULL;
}

int MyListInit(MyList *list) {
    list->num_members = 0;
    MyListElem *anchor_ptr = (MyListElem *) malloc(sizeof(MyListElem));
    anchor_ptr->obj = NULL;
    list->anchor = *anchor_ptr;
    anchor_ptr->next = anchor_ptr;
    anchor_ptr->prev = anchor_ptr;
    return TRUE;
}
