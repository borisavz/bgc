#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct object_t object;
typedef struct list_node_t list_node;
typedef struct list_t list;

struct object_t {
    list *refs;
};

struct list_node_t {
    void *element;
    list_node *next;
    list_node *previous;
};

struct list_t {
    list_node *first;
    list_node *last;
    int size;
};

void add_first(list *list, void *element) {
    list_node *node = malloc(sizeof(list_node));

    node->element = element;
    node->previous = NULL;
    node->next = list->first;

    if(list->size == 0) {
        list->last = node;
    } else {
        list->first->previous = node;
    }

    list->first = node;
    list->size++;
}

void add_last(list *list, void *element) {
    list_node *node = malloc(sizeof(list_node));

    node->element = element;
    node->previous = list->last;
    node->next = NULL;

    if(list->size == 0) {
        list->first = node;
    } else {
        list->last->next = node;
    }

    list->last = node;
    list->size++;
}

void remove_last(list *list) {
    if(list->size == 0) {
        return;
    }

    list->last = list->last->previous;
    
    if(list->last != NULL) {
        list->last->next = NULL;
    }

    /*TODO: memory leak*/

    list->size--;
}

bool is_empty(list *list) {
    return list->size == 0;
}

list *new_list() {
    list *new_list = malloc(sizeof(list));

    new_list->first = NULL;
    new_list->last = NULL;
    new_list->size = 0;

    return new_list;
}

int main() {
    list *a = new_list();

    for(int i = 0; i < 5; i++) {
        int *x = malloc(sizeof(int));
        *x = i;
        add_last(a, x);
    }

    for(int i = 0; i < 5; i++) {
        int x = *((int *) a->last->element);
        printf("a[%d] = %d\n", i, x);
        remove_last(a);
    }
}