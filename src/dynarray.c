#include "dynarray.h"

DynArray_Result dynarray_alloc(size_t item_size, size_t capacity) {
    DynArray array = {0}; 
    array.dyn_itemsize = item_size;
    array.dyn_capacity = capacity;
    array.dyn_count = 0;

    array.items = NULL;

    if (array.dyn_capacity == 0) {
        goto created;
    }

    char* items = malloc(array.dyn_capacity * array.dyn_itemsize);
    if (items == NULL) {
        DynArray_Result result = {
            .error = NoMemory,
            .data = {0}
        };
        return result;
    }

    memset(items, 0, array.dyn_capacity * array.dyn_itemsize);
    array.items = items;

created:
{
    DynArray_Result result = {
        .error = Created,
        .data = { .array = array }
    };
    return result;
}
}

DynArray_Result dynarray_grow_amortized(DynArray *array, size_t additional) {
    size_t exact_capacity = array->dyn_count + additional;

    size_t new_capacity = array->dyn_capacity * 2 > exact_capacity ?
        array->dyn_capacity * 2 :
        exact_capacity;
    new_capacity = new_capacity > 4 ? new_capacity : 4;

    char* items = realloc(array->items, array->dyn_itemsize * new_capacity);
    if (items == NULL) {
        DynArray_Result result = {
            .error = NoMemory,
            .data = {0}
        };
        return result;
    }

    array->items = items;
    array->dyn_capacity = new_capacity;

    DynArray_Result result = {
        .error = Grew,
        .data = {0}
    };
    return result;
}

DynArray_Result dynarray_get_at_error(DynArray *array, size_t index) {
    if (index >= array->dyn_count) {
        DynArray_Result result = {
            .error = OutOfBounds,
            .data = {0}
        };
        return result;
    }

    char* data = array->items + (index * array->dyn_itemsize);

    DynArray_Result result = {
        .error = Retrieved,
        .data = { .data = data }
    };
    return result;
}

DynArray_Result dynarray_set_at_error(DynArray *array, size_t index, const void *item) {
    if (index >= array->dyn_count) {
        DynArray_Result result = {
            .error = OutOfBounds,
            .data = {0}
        };
        return result;
    }

    char* data = array->items + (index * array->dyn_itemsize);
    memcpy(data, item, array->dyn_itemsize);

    DynArray_Result result = {
        .error = Wrote,
        .data = {0}
    };
    return result;
}

DynArray_Result dynarray_remove_at(DynArray *array, size_t index) {
    if (index >= array->dyn_count) {
        DynArray_Result result = {
            .error = OutOfBounds,
            .data = {0}
        };
        return result;
    }

    char* item_offset = array->items + (array->dyn_itemsize * index);
    char* items_rest = item_offset + array->dyn_itemsize;
    
    size_t remaining_items = array->dyn_count - index - 1;

    memcpy(items_rest, item_offset, remaining_items * array->dyn_itemsize);

    array->dyn_count--;
    memset(
            item_offset + (remaining_items * array->dyn_itemsize),
            0, array->dyn_itemsize);

    DynArray_Result result = {
        .error = Removed,
        .data = {0}
    };
    return result;
}

void dynarray_free(DynArray *array) {
    free(array->items);
    array->items = NULL;
    array->dyn_capacity = 0;
    array->dyn_count = 0;
}

