#ifndef TESTS_H
#define TESTS_H

#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <ctype.h>
#include "../sco.h"

#ifdef __clang__
#pragma GCC diagnostic ignored "-Wcompound-token-split-by-macro"
#endif

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#define STACK_SIZE SCO_MINSTACKSIZE

static atomic_size_t total_allocs = 0;
static atomic_size_t total_mem = 0;

static void *xmalloc(size_t size) {
    void *mem = malloc(16+size);
    assert(mem);
    *(size_t*)mem = size;
    atomic_fetch_add(&total_allocs, 1);
    atomic_fetch_add(&total_mem, size);
    return ((char*)mem+16);
}

static void xfree(void *mem) {
    if (mem) {
        size_t size = *(size_t*)(((char*)mem)-16);
        free(((char*)mem)-16);
        atomic_fetch_sub(&total_allocs, 1);
        atomic_fetch_sub(&total_mem, size);
    }
}

#define do_test(name) { \
    if (argc < 2 || strstr(#name, argv[1])) { \
        printf("%s\n", #name); \
        name(); \
        cleanup_test_allocator(); \
    } \
}

void cleanup_test_allocator(void) {
    size_t total_allocs0 = atomic_load(&total_allocs);
    size_t total_mem0 = atomic_load(&total_mem);
    if (total_allocs0 > 0 || total_mem0 > 0) {
        fprintf(stderr, "test failed: %zu unfreed allocations, %zu bytes\n",
            total_allocs0, total_mem0);
        exit(1);
    }
}

static __thread int started = 0;
static __thread int cleaned = 0;

#define reset_stats() { \
    started = 0; \
    cleaned = 0; \
}

#define quick_start(entry_, cleanup_, udata_) { \
    void *stack = xmalloc(STACK_SIZE); \
    assert(stack); \
    started++; \
    sco_start(&(struct sco_desc){  \
        .stack = stack, \
        .stack_size = STACK_SIZE, \
        .entry = (entry_), \
        .udata = (udata_), \
        .cleanup = (cleanup_), \
    }); \
}

static void co_cleanup(void *stack, size_t stack_size, void *udata) {
    (void)udata;
    assert(stack_size == STACK_SIZE);
    assert(stack);
    xfree(stack);
    cleaned++;
}

static int64_t getnow(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec*INT64_C(1000000000) + now.tv_nsec);
}

static void sco_sleep(int64_t nanosecs) {
    int64_t start = getnow();
    while (getnow()-start < nanosecs) {
        sco_yield();
    }
}

#endif // TESTS_H


