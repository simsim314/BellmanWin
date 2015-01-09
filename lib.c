#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "lib.h"

void *allocate(size_t sz) {
        void *p = malloc(sz);
        if(!p) {
                fprintf(stderr, "out of memory\n");
                return NULL;
        }
        memset(p, 0, sz);
        return p;
}
