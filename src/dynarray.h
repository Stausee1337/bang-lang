#ifndef DYNARRAY_H_
#define DYNARRAY_H_

#include <stdio.h>
#include <assert.h>
#include <string.h>

#define DA_INIT_CAP 4

#define da_append(da, item)                                                          \
    do {                                                                             \
        if ((da)->count >= (da)->capacity) {                                         \
            (da)->capacity = (da)->capacity == 0 ? DA_INIT_CAP : (da)->capacity*2;   \
            (da)->items = realloc((da)->items, (da)->capacity*sizeof(*(da)->items)); \
            assert((da)->items != NULL && "Buy more RAM lol");                       \
        }                                                                            \
                                                                                     \
        (da)->items[(da)->count++] = (item);                                         \
    } while (0)

#define da_append_many(da, new_items, new_items_count)                                       \
    do {                                                                                     \
        if ((da)->count + new_items_count > (da)->capacity) {                                \
            if ((da)->capacity == 0) {                                                       \
                (da)->capacity = DA_INIT_CAP;                                                \
            }                                                                                \
            while ((da)->count + new_items_count > (da)->capacity) {                         \
                (da)->capacity *= 2;                                                         \
            }                                                                                \
            (da)->items = realloc((da)->items, (da)->capacity*sizeof(*(da)->items));         \
            assert((da)->items != NULL && "Buy more RAM lol");                               \
        }                                                                                    \
        memcpy((da)->items + (da)->count, new_items, new_items_count*sizeof(*(da)->items));  \
        (da)->count += new_items_count;                                                      \
    } while (0)


#endif // DYNARRAY_H_
