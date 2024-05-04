#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct object_t object;
typedef struct list_node_t list_node;
typedef struct list_t list;

struct object_t {
    bool marked;
    int id;
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

list *all_objects;

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

void add_all(list *target, list *source) {
    if(source->size == 0) {
        return;
    }

    list_node *curr_node = source->first;

    do {
        add_last(target, curr_node->element);
    } while((curr_node = curr_node->next) != NULL);
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

object *new_object() {
    static int id = 0;

    object *new_object = malloc(sizeof(object));

    new_object->marked = false;
    new_object->id = id;
    new_object->refs = new_list();

    id++;
    add_last(all_objects, new_object);

    return new_object;
}

void tc_mark_sweep(list *roots) {
    list *worklist = new_list();

    add_all(worklist, roots);

    while(worklist->size != 0) {
        object *obj = (object *) worklist->last->element;
        remove_last(worklist);

        if(obj->marked == false) {
            obj->marked = true;
            add_all(worklist, obj->refs);
        }
    }

    list_node *curr_node = all_objects->first;

    do {
        object *obj = (object *) curr_node->element;
        
        if(obj->marked == false) {
            printf("delete object %d\n", obj->id);
        }
    } while((curr_node = curr_node->next) != NULL);
}

int main() {
    all_objects = new_list();

    list *roots = new_list();

    object *a = new_object();
    object *b = new_object();
    object *c = new_object();
    object *d = new_object();

    add_first(a->refs, b);
    add_first(a->refs, c);

    add_first(roots, a);

    tc_mark_sweep(roots);

    return 0;
}