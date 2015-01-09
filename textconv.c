#include <string.h>
#include "universe.h"
#include "lib.h"

char tile_get_text(tile *t, int x, int y) {
        if(!t->text) return ' ';
        else return t->text[(y * TILE_WIDTH) + x];
}

void tile_set_text(tile *t, int x, int y, char c) {
        if(!t->text) {
                t->text = (char *)allocate(TILE_WIDTH * TILE_HEIGHT);
                memset(t->text, ' ', TILE_WIDTH * TILE_HEIGHT);
        }
        t->text[(y * TILE_WIDTH) + x] = c;
}

char generation_get_text(generation *g, int x, int y) {
        tile *t = generation_find_tile(g, x, y, 1);
        return tile_get_text(t, x % TILE_WIDTH, y % TILE_HEIGHT);
}

void generation_set_text(generation *g, int x, int y, char c) {
        tile *t = generation_find_tile(g, x, y, 1);
        tile_set_text(t, x % TILE_WIDTH, y % TILE_HEIGHT, c);
}

void generation_to_text(generation *g) {
        tile *t;
        int x, y;

        for(t = g->all_first; t; t = t->all_next) {
                for(x=0; x<TILE_WIDTH; x++) for(y=0; y<TILE_HEIGHT; y++) {
                        cellvalue v = tile_get_cell(t, x, y);
                        char c = '.';
                        if(v != OFF)
                                c = '*';
                        tile_set_text(t, x, y, c);
                }
        }
}

void tile_find_bounds_text(tile *t, int *l, int *r, int *t_, int *b) {
        int ymin = TILE_HEIGHT, ymax = 0;
        int xmin = TILE_WIDTH, xmax = 0;
        int x, y;

        for(y=0; y<TILE_HEIGHT; y++) {
                for(x=0; x<TILE_WIDTH; x++) {
                        char c = tile_get_text(t, x, y);

                        if(c != ' ') {
                                if(y > ymax) ymax = y;
                                if(y < ymin) ymin = y;

                                if(x > xmax) xmax = x;
                                if(x < xmin) xmin = x;
                        }
                }
        }

        *l = xmin;
        *r = xmax;
        *t_ = ymin;
        *b = ymax;
}

