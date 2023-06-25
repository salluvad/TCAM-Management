#ifndef __LIST_HEADER__
#define __LIST_HEADER__
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>


typedef struct free_fragment_ {
    uint32_t start; // Start position of the free fragment
    uint32_t end;   // End position of the free fragment
    struct free_fragment_ *next; // Pointer to the next free fragment
} free_fragment_t;

// Global free fragment list
free_fragment_t *free_fragments;

void add_free_fragment(uint32_t start, uint32_t end);

free_fragment_t *find_free_fragment(uint32_t size);

void remove_free_fragment(free_fragment_t *fragment);

void destroy_free_fragments();

uint32_t free_fragment_list_size();
#endif
