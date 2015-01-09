#include <stdlib.h>
#include <assert.h>
#include <getopt.h>
#include "universe.h"
#include "readwrite.h"

int main(int argc, char *argv[]) {

        int c;
        int gens = 10;

        while((c = getopt(argc, argv, "g:")) != -1) switch(c) {
        case 'g':
                gens = strtoul(optarg, NULL, 10);
                break;
        }

        universe *u = read_text(argv[optind]);

        tile *t;
        int x, y;

        for(t=u->first->all_first; t; t=t->all_next) {
                for(y=0; y<TILE_HEIGHT; y++) for(x=0; x<TILE_WIDTH; x++) {
                        cellvalue v = tile_get_cell(t, x, y);
                        if(v != OFF)
                                tile_set_cell(t, x, y, ON);
                }
        }

        int i;
        for(i=0; i<gens; i++)
                universe_evolve_next(u);

        generation_to_text(u->first);

        for(t=u->first->all_first; t; t=t->all_next) {
                for(y=0; y<TILE_HEIGHT; y++) for(x=0; x<TILE_WIDTH; x++) {
                        char v = tile_get_text(t, x, y);
                        if(v == '.')
                                tile_set_text(t, x, y, ' ');
                }
        }

        generation *g;

        for(g = u->first; g; g = g->next) {
                for(t = g->all_first; t; t = t->next) {
                        for(y=0; y<TILE_HEIGHT; y++) for(x=0; x<TILE_WIDTH; x++) {
                                cellvalue v = tile_get_cell(t, x, y);
                                if(v == ON) {
                                        int dx, dy;
                                        for(dx = -1; dx <= 1; dx++) for(dy = -1; dy <= 1; dy++) {
                                                int xx = t->xpos + x + dx;
                                                int yy = t->ypos + y + dy;
                                                c = generation_get_text(u->first, xx, yy);
                                                if(c == ' ')
                                                        generation_set_text(u->first, xx, yy, '.');
                                        }
                                }
                        }                        
                }
        }

        write_life105_text(stdout, u->first);

        return 0;
}

