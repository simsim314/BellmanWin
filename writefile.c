#include <string.h>
#include <stdio.h>

#include "readwrite.h"

void write_life105(FILE *f, generation *g) {
        tile *t;
        int x, y;

        for(t=g->all_first; t; t=t->all_next) {
                int ll, rr, tt, bb;

                tile_find_bounds(t, &ll, &rr, &tt, &bb);


                fprintf(f, "#P %d %d\n", t->xpos + ll, t->ypos + tt);

                for(y=tt; y<=bb; y++) {
                        for(x=ll; x<=rr; x++) {
                                char c = '.';
                                switch(tile_get_cell(t, x, y)) {
                                case OFF: c = '.'; break;
                                case ON: c = '*'; break;
                                case UNKNOWN: c = ':'; break;
                                case UNKNOWN_STABLE: c = '?'; break;
                                }
                                fputc(c, f);
                        }
                        fputc('\n', f);
                }
        }
}

void write_life105_text(FILE *f, generation *g) {
        tile *t;
        int x, y;

        for(t=g->all_first; t; t=t->all_next) {
                int ll, rr, tt, bb;

                tile_find_bounds_text(t, &ll, &rr, &tt, &bb);

                fprintf(f, "#P %d %d\n", t->xpos + ll, t->ypos + tt);

                for(y=tt; y<=bb; y++) {
                        for(x=ll; x<=rr; x++) {
                                fputc(tile_get_text(t, x, y), f);
                        }
                        fputc('\n', f);
                }
        }
}
