#include <stdlib.h>
#include <assert.h>
#include "universe.h"
#include "readwrite.h"

int main(int argc, char *argv[]) {

        assert(argc > 1);
        universe *u = read_text(argv[1]);

        find_still_life(u);

        return 0;
}

