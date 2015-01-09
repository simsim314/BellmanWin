#include <assert.h>
#include "universe.h"

// Simple evolver for testing purposes, which only handles OFF and ON
// values, and assumes the default background value is OFF.

static cellvalue tile_get_cell_wrap(tile *t, int xpos, int ypos) {
        // first check for the 4 corners
        if((xpos == -1) && (ypos == -1)) {
                tile *t2;
                if(t->left && t->left->up) t2 = t->left->up;
                else if(t->up && t->up->left) t2 = t->up->left;
                else return OFF; 
                return tile_get_cell(t2, xpos, ypos);
        }

        if((xpos == TILE_WIDTH) && (ypos == -1)) {
                tile *t2;
                if(t->right && t->right->up) t2 = t->right->up;
                else if(t->up && t->up->right) t2 = t->up->right;
                else return OFF; 
                return tile_get_cell(t2, xpos, ypos);
        }

        if((xpos == -1) && (ypos == TILE_HEIGHT)) {
                tile *t2;
                if(t->left && t->left->down) t2 = t->left->down;
                else if(t->down && t->down->left) t2 = t->down->left;
                else return OFF; 
                return tile_get_cell(t2, xpos, ypos);
        }

        if((xpos == TILE_WIDTH) && (ypos == TILE_HEIGHT)) {
                tile *t2;
                if(t->right && t->right->down) t2 = t->right->down;
                else if(t->down && t->down->right) t2 = t->down->right;
                else return OFF; 
                return tile_get_cell(t2, xpos, ypos);
        }

        // now check for the 4 edges
        if(xpos == -1) {
                if(t->left) return tile_get_cell(t->left, xpos, ypos);
                else return OFF;
        }

        if(xpos == TILE_WIDTH) {
                if(t->right) return tile_get_cell(t->right, xpos, ypos);
                else return OFF;
        }

        if(ypos == -1) {
                if(t->up) return tile_get_cell(t->up, xpos, ypos);
                else return OFF;
        }

        if(ypos == TILE_HEIGHT) {
                if(t->down) return tile_get_cell(t->down, xpos, ypos);
                else return OFF;
        }

        return tile_get_cell(t, xpos, ypos);
}

evolve_result tile_evolve_simple(tile *t, tile *out) {
        uint32_t x, y; 
        for(x=0; x<TILE_WIDTH; x++) {
                for(y=0; y<TILE_HEIGHT; y++) {
                        // Count neighbour values
                        int ncount[4] = { 0, 0, 0, 0 };
                        ncount[tile_get_cell_wrap(t, x-1, y-1)]++;
                        ncount[tile_get_cell_wrap(t, x,   y-1)]++;
                        ncount[tile_get_cell_wrap(t, x+1, y-1)]++;
                        ncount[tile_get_cell_wrap(t, x-1, y)  ]++;
                        ncount[tile_get_cell_wrap(t, x+1, y)  ]++;
                        ncount[tile_get_cell_wrap(t, x-1, y+1)]++;
                        ncount[tile_get_cell_wrap(t, x,   y+1)]++;
                        ncount[tile_get_cell_wrap(t, x+1, y+1)]++;

                        // now apply the life rule
                        cellvalue orig = tile_get_cell(t, x, y);
                        cellvalue result = OFF;
                        if(((orig == ON) && ((ncount[ON] == 2) || (ncount[ON] == 3))) ||
                           ((orig == OFF) && (ncount[ON] == 3)))
                                result = ON;
                        
                        tile_set_cell(out, x, y, result);
                }
        }

        // now see if we should expand
        evolve_result r = 0;
        for(x=0; x<TILE_WIDTH; x++) {
                if(tile_get_cell(t, x, 0) == ON)
                        r |= EXPAND_UP;
                if(tile_get_cell(t, x, TILE_HEIGHT-1) == ON)
                        r |= EXPAND_DOWN;
        }
        for(y=0; y<TILE_HEIGHT; y++) {
                if(tile_get_cell(t, 0, y) == ON)
                        r |= EXPAND_LEFT;
                if(tile_get_cell(t, TILE_WIDTH-1, y) == ON)
                        r |= EXPAND_RIGHT;
        }
        return r;
}
