#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

bool stw_end = false;
pthread_mutex_t stw_end_m = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t stw_end_c = PTHREAD_COND_INITIALIZER;

int safepoint_ack_count = 0;
pthread_mutex_t safepoint_m = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t safepoint_c = PTHREAD_COND_INITIALIZER;

typedef struct object_t object;
typedef struct list_node_t list_node;
typedef struct list_t list;
typedef struct mutator_info_t mutator_info;

bool stw_call = false;
pthread_mutex_t stw_call_m = PTHREAD_MUTEX_INITIALIZER;

bool gc_active = false;
pthread_mutex_t gc_active_m = PTHREAD_MUTEX_INITIALIZER;

struct mutator_info_t {
    list *roots;
};

void mutator_thread(mutator_info *info);
void safepoint(mutator_info *info, bool invalidate);
void stw();

mutator_info mutators[5];

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
pthread_mutex_t all_objects_m = PTHREAD_MUTEX_INITIALIZER;

list *worklist;
pthread_mutex_t worklist_m = PTHREAD_MUTEX_INITIALIZER;

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

    pthread_mutex_lock(&all_objects_m);
    add_last(all_objects, new_object);
    pthread_mutex_unlock(&all_objects_m);

    return new_object;
}

void add_ref(object *target, object *obj) {
    add_last(target->refs, obj);

    pthread_mutex_lock(&gc_active_m);

    if(gc_active == true) {
        printf("write barrier\n");
        obj->marked = true;

        pthread_mutex_lock(&worklist_m);
        add_last(worklist, obj);
        pthread_mutex_unlock(&worklist_m);
    }
    
    pthread_mutex_unlock(&gc_active_m);
}

void tc_mark_sweep(list *roots) {
    worklist = new_list();

    add_all(worklist, roots);

    while(worklist->size != 0) {
        pthread_mutex_lock(&worklist_m);

        if(worklist->size == 0) {
            pthread_mutex_unlock(&worklist_m);
            break;
        }

        object *obj = (object *) worklist->last->element;
        remove_last(worklist);

        pthread_mutex_unlock(&worklist_m);

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

        obj->marked = false;
    } while((curr_node = curr_node->next) != NULL);

    pthread_mutex_lock(&gc_active_m);
    gc_active = false;
    pthread_mutex_unlock(&gc_active_m);
}

int main() {
    all_objects = new_list();

    pthread_t threads[5];
    
    for(int i = 0; i < 5; i++) {
        mutators[i].roots = new_list();

        pthread_create(&threads[i], NULL, &mutator_thread, &mutators[i]);
    }

    sleep(1);

    stw();

    sleep(4);

    stw();

    for(int i = 0; i < 5; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}

void mutator_thread(mutator_info *info) {
    pthread_t self_thread = pthread_self();

    object *a = new_object();
    object *b = new_object();
    object *c = new_object();
    object *d = new_object();

    add_ref(a, b);
    add_ref(a, c);

    add_first(info->roots, a);

    printf("enter mutator %ld\n", self_thread);
    
    printf("start mutator logic 1 %ld\n", self_thread);
    sleep(10);
    printf("end mutator logic 1 %ld\n", self_thread);
    
    safepoint(info, true);
    
    printf("start mutator logic 2 %ld\n", self_thread);
    sleep(5);
    object *e = new_object();
    object *f = new_object();
    add_ref(a, e);
    printf("end mutator logic 2 %ld\n", self_thread);
    
    safepoint(info, false);
    printf("exit mutator %ld\n", self_thread);
}

void safepoint(mutator_info *info, bool invalidate) {
    pthread_mutex_lock(&stw_call_m);
    bool stw_call_copy = stw_call;
    pthread_mutex_unlock(&stw_call_m);

    pthread_mutex_lock(&safepoint_m);
    safepoint_ack_count++;
    pthread_cond_signal(&safepoint_c);
    pthread_mutex_unlock(&safepoint_m);

    if(stw_call_copy) {
        printf("start wait stw end");

        pthread_mutex_lock(&stw_end_m);
        while (stw_end == false) {
            pthread_cond_wait(&stw_end_c, &stw_end_m);
        }
        pthread_mutex_unlock(&stw_end_m);

        printf("end wait stw end");
    }

    if(invalidate) {
        pthread_mutex_lock(&safepoint_m);
        safepoint_ack_count--;
        pthread_mutex_unlock(&safepoint_m);
    }
}

void stw() {
    printf("enter stw\n");

    pthread_mutex_lock(&stw_end_m);
    stw_end = false;
    pthread_mutex_unlock(&stw_end_m);

    printf("begin stw call\n");

    pthread_mutex_lock(&stw_call_m);
    stw_call = true;
    pthread_mutex_unlock(&stw_call_m);

    pthread_mutex_lock(&gc_active_m);
    gc_active = true;
    pthread_mutex_unlock(&gc_active_m);

    printf("end stw call\n");
    printf("begin wait safepoint\n");
    
    pthread_mutex_lock(&safepoint_m);
    while (safepoint_ack_count != 5) {
        pthread_cond_wait(&safepoint_c, &safepoint_m);
    }
    pthread_mutex_unlock(&safepoint_m);

    pthread_mutex_lock(&stw_call_m);
    stw_call = false;
    pthread_mutex_unlock(&stw_call_m);

    printf("end wait safepoint\n");
    printf("start stw logic\n");

    sleep(5);

    list *all_roots = new_list();

    for(int i = 0; i < 5; i++) {
        add_all(all_roots, mutators[i].roots);
    }

    printf("end stw logic\n");

    pthread_mutex_lock(&stw_end_m);
    stw_end = true;
    pthread_cond_broadcast(&stw_end_c);
    pthread_mutex_unlock(&stw_end_m);

    printf("exit stw\n");

    pthread_t gc_background;
    pthread_create(&gc_background, NULL, &tc_mark_sweep, all_roots);
    pthread_join(gc_background, NULL);
}