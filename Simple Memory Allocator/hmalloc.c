
#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "hmalloc.h"

typedef struct node {

    size_t size;
    struct node * next;

}
node;

const size_t PAGE_SIZE = 4096;

static hm_stats stats; // This initializes the stats to 0.

node * f_node;

// This is used as a helper to free_list_length
long
free_list_length_help(node * node) {

    if (node-> next == 0) {

        return 1;
    }

    return 1 + free_list_length_help(node-> next);
}

// This is used to calculate the length of the free list
long
free_list_length() {

    if (f_node == 0) {

        return 0;

    } else {

        return free_list_length_help(f_node);

    }

}

// This is used to take desired cell
node *
    take_cell(size_t size) {

        for (node * node = f_node; node != 0; node = node-> next) {

            if (node-> size >= size) {

                return node;

            }

        }

        return 0;

    }

// This is used to coalesce our free list(node)
void
coalesce_free_list() {

    for (node * node = f_node; node != 0; node = node-> next) {

        if (node-> next == 0) {

            break;

        }

        while (((long) node) + node-> size == ((long) node-> next)) {

            node-> size += node-> next-> size;

            node-> next = node-> next-> next;

        }

    }

}

void
add(node * n) {
    node * n_n;

    for (n_n = f_node;
        ((long) n_n-> next) < ((long) n); n_n = n_n-> next) {

        if (n_n-> next == 0) {

            break;

        }

    }

    if (n_n > n) {

        n-> next = f_node;

        f_node = n;

    } else {

        n-> next = n_n-> next;

        n_n-> next = n;

    }

    coalesce_free_list();

}

hm_stats*
hgetstats()
{
    stats.free_length = free_list_length();
    return &stats;
}

void
hprintstats()
{
    stats.free_length = free_list_length();
    fprintf(stderr, "\n== husky malloc stats ==\n");
    fprintf(stderr, "Mapped:   %ld\n", stats.pages_mapped);
    fprintf(stderr, "Unmapped: %ld\n", stats.pages_unmapped);
    fprintf(stderr, "Allocs:   %ld\n", stats.chunks_allocated);
    fprintf(stderr, "Frees:    %ld\n", stats.chunks_freed);
    fprintf(stderr, "Freelen:  %ld\n", stats.free_length);
}

static
size_t
div_up(size_t xx, size_t yy)
{
    // This is useful to calculate # of pages
    // for large allocations.
    size_t zz = xx / yy;

    if (zz * yy == xx) {
        return zz;
    }
    else {
        return zz + 1;
    }
}

void *
    hmalloc(size_t size) {
        stats.chunks_allocated += 1;

        size += sizeof(size_t);

        void * loc;

        int t = sizeof(size_t);

        if (size < PAGE_SIZE) {

            if (f_node == 0) {

                stats.pages_mapped += 1;

                f_node = mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

                f_node-> size = PAGE_SIZE - t;

                f_node-> next = 0;

            }

            node * n = take_cell(size);
            if (n) {

                node * nex = n-> next;

                node * n_n = ((void * ) n) + size;

                n_n-> size = n-> size - size;

                n_n-> next = nex;

                *((size_t * ) n) = size;

                loc = ((void * ) n) + t;

                if (n == f_node) {

                    f_node = n_n;

                } else {

                    node * n_f;

                    for (n_f = f_node;
                        ((long) n_f-> next) != ((long) n); n_f = n_f-> next) {}

                    n_f-> next = n_n;

                }

            } else {

                stats.pages_mapped += 1;

                void * map = mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

                *((size_t * ) map) = size;

                loc = map + t;

                node * n_n = (node * )((void * ) map + size);

                n_n-> size = PAGE_SIZE - size;

                n_n-> next = 0;

                add(n_n);

            }

        } else {

            size_t num_pages = div_up(size, PAGE_SIZE);

            stats.pages_mapped += num_pages;

            size = num_pages * PAGE_SIZE;

            void * map = mmap(0, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

            *((size_t * ) map) = size;

            loc = map + t;

        }

        memset(loc, 0, size - t);

        return loc;

    }

void
hfree(void * item) {

    stats.chunks_freed += 1;

    int t = sizeof(size_t);

    node * n = (node * )(item - t);

    size_t size = * ((size_t * ) n);

    if (size < PAGE_SIZE) {

        n-> size = size;

        n-> next = 0;

        add(n);

    } else {

        stats.pages_unmapped += div_up(size, PAGE_SIZE);

        munmap(n, size);

    }

}

