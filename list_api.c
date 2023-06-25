#include "list_api.h"

// add a free fragment to the list
void add_free_fragment(uint32_t start, uint32_t end) {
    free_fragment_t *new_fragment = malloc(sizeof(free_fragment_t));
    new_fragment->start = start;
    new_fragment->end = end;
    new_fragment->next = NULL;

    if (free_fragments == NULL) {
        free_fragments = new_fragment;
    } else {
        free_fragment_t *current = free_fragments;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_fragment;
    }
}

// find a free fragment for insertion
free_fragment_t *find_free_fragment(uint32_t size) {
    free_fragment_t *current = free_fragments;
    free_fragment_t *prev = NULL;

    while (current != NULL) {
        uint32_t fragment_size = current->end - current->start + 1;
        if (fragment_size >= size) {
            // Fragment is large enough, break it into two if necessary
            if (fragment_size > size) {
                free_fragment_t *new_fragment = malloc(sizeof(free_fragment_t));
                new_fragment->start = current->start + size;
                new_fragment->end = current->end;
                new_fragment->next = current->next;
                current->end = current->start + size - 1;
                current->next = new_fragment;
            }

          /*  // Remove the fragment from the free fragment list
            if (prev == NULL) {
                free_fragments = current->next;
            } else {
                prev->next = current->next;
            }
*/
            return current;
        }

        prev = current;
        current = current->next;
    }

    return NULL;
}

// remove a free fragment from the list
void remove_free_fragment(free_fragment_t *fragment) {
    if (fragment == NULL) {
        return;
    }

    free_fragment_t *current = free_fragments;
    free_fragment_t *prev = NULL;

    while (current != NULL) {
        if (current == fragment) {
            // Remove the fragment from the free fragment list
            if (prev == NULL) {
                free_fragments = current->next;
            } else {
                prev->next = current->next;
            }

            free(current);
            return;
        }

        prev = current;
        current = current->next;
    }
}

// destroy the free fragment list
void destroy_free_fragments() {
    free_fragment_t *current = free_fragments;

    while (current != NULL) {
        free_fragment_t *temp = current;
        current = current->next;
        free(temp);
    }

    free_fragments = NULL;
}

// size of the list
uint32_t free_fragment_list_size() {
    uint32_t size = 0;
    free_fragment_t *curr = free_fragments;

    while (curr != NULL) {
        size ++;;
        curr = curr->next;
    }

    return size;
}
 
