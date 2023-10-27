#ifndef DYNARRAY_H_
#define DYNARRAY_H_

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

typedef struct {
    size_t dyn_count;
    size_t dyn_capacity;
    size_t dyn_itemsize;

    char *items;
} DynArray;

typedef enum {
    Created,
    Wrote,
    Grew,
    Retrieved,
    Removed,
    NoMemory,
    OutOfBounds,
} DynArray_Error;

typedef struct {
    DynArray_Error error;
    union {
        DynArray array;
        void* data;
    } data;
} DynArray_Result;

#define dynarray_create(type)   dynarray_alloc(sizeof(type), 0)
#define dynarray_create_with_capacity(type, capacity) \
    dynarray_alloc(sizeof(type), capacity)

#define dynarray_push(arr, item) \
    do { \
        typeof(arr) array = arr; \
        assert(array->dyn_itemsize == sizeof(item) && \
                "DynArray: Item sizes don't match up"); \
        if (array->dyn_count == array->dyn_capacity) { \
            _handle_possible_error(dynarray_grow_amortized(array, 1), Grew);\
        } \
        char* end = array->items + (array->dyn_count * array->dyn_itemsize); \
        memcpy(end, &item, array->dyn_itemsize);\
        array->dyn_count++; \
    } while(0)

#define dynarray_get_at(itype, arr, index) \
({ typeof(arr) array = arr; \
    assert(array->dyn_itemsize == sizeof(itype) && \
          "DynArray: Item sizes don't match up"); \
    _handle_possible_error(dynarray_get_at_error(array, index), Retrieved); \
})

#define dynarray_set_at(arr, index, it) \
    do { \
        typeof(it) item = it; \
        typeof(arr) array = arr; \
        assert(array->dyn_itemsize == sizeof(item) && \
              "DynArray: Item sizes don't match up"); \
        _handle_possible_error( \
                dynarray_set_at_error(array, index, &item), Wrote); \
    } while(0)

DynArray_Result dynarray_alloc(size_t item_size, size_t capacity);
DynArray_Result dynarray_grow_amortized(DynArray *array, size_t additional);
DynArray_Result dynarray_get_at_error(DynArray *array, size_t index);
DynArray_Result dynarray_set_at_error(DynArray *array, size_t index, const void* data);
DynArray_Result dynarray_remove_at(DynArray *array, size_t index);
void dynarray_free(DynArray *array);

static inline
void* _handle_possible_error(DynArray_Result result, DynArray_Error expc) {
    if (result.error == expc) {
        if (result.error == Retrieved) {
            return result.data.data;
        }
        return 0;
    }
    switch (result.error) {
        case OutOfBounds:
            assert(false && "Array Access: Out of Bounds");
        case NoMemory:
            assert(false && "Array Allocation: No Memory for Items");
        default:
            assert(false && "Forgot to handle array error");
    }

}



#endif // DYNARRAY_H_
