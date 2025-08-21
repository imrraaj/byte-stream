#ifndef MACROS_H
#define MACROS_H

#include <stdlib.h>
#include <assert.h>

#define ARRAY_LEN(arr) ((size_t)(sizeof(arr) / sizeof((arr)[0])))
#define da_append(da, item)                                                        \
    do                                                                             \
    {                                                                              \
        if ((da).count >= (da).capacity)                                           \
        {                                                                          \
            (da).capacity = (da).capacity ? (da).capacity * 2 : 64;                \
            (da).items = realloc((da).items, sizeof(*(da).items) * (da).capacity); \
            assert((da).items != NULL);                                            \
        }                                                                          \
        (da).items[(da).count++] = (item);                                         \
    } while (0)

#endif // MACROS_H
