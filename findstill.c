#include <stdlib.h>
#include <assert.h>
#include "universe.h"
#include "readwrite.h"

static uint32_t n_set, n_best;
static universe *best;

#define PRUNE(x) //printf x

static void recurse(universe *u, universe *init) {

        // first of all, are there any cells we can determine immediately?

        tile *t;
        int flag = 0;
        for(t = u->first->all_first; t; t = t->all_next) {
                if(!t->next) {
                        t->next = generation_find_tile(u->first->next, t->xpos, t->ypos, 1);
                }
                evolve_result res = tile_stabilise_3state(t, t->next);
           
                if(res & ABORT) {
                        PRUNE(("Abort\n"));
                        return;
                } else if(res & ACTIVE) {
                        flag = 1;
                }
        }

        if(flag) {
                // Count the changed words
                int i, count = 0;
                for(t = u->first->all_first; t; t = t->all_next) {
                        for(i=0; i<TILE_HEIGHT; i++) {
                                if((t->next->bit0[i] != t->bit0[i]) ||
                                   (t->next->bit1[i] != t->bit1[i]))
                                        count++;
                        }
                }

                struct backout {
                        tile *tileptr;
                        int index;
                        TILE_WORD old0, old1;
                } *backout;

                backout = (struct backout *)alloca(count * sizeof *backout);
                count = 0;

                // store old values of the changed words

                for(t = u->first->all_first; t; t = t->all_next) {
                        for(i=0; i<TILE_HEIGHT; i++) {
                                if((t->next->bit0[i] != t->bit0[i]) ||
                                   (t->next->bit1[i] != t->bit1[i])) {
                                        backout[count].tileptr = t;
                                        backout[count].index = i;
                                        backout[count].old0 = t->bit0[i];
                                        backout[count].old1 = t->bit1[i];
                                        t->bit0[i] = t->next->bit0[i];
                                        t->bit1[i] = t->next->bit1[i];
                                        count++;
                                }
                        }
                }

                // now recurse
                //printf("Recorded %d, recursing...\n", count);
                recurse(u, init);

                // back out the changes
                for(i=0; i<count; i++) {
                        tile *tt = backout[i].tileptr;
                        tt->bit0[backout[i].index] = backout[i].old0;
                        tt->bit1[backout[i].index] = backout[i].old1;
                }

                // and continue
                //printf("Backed out %d\n", count);
                return;
        }

        // Find first tile/word with an unknown cell
        int i;
        tile *f = NULL;
        for(t = u->first->all_first; t && !f; t = t->all_next) {
                for(i=0; i<TILE_HEIGHT; i++) {
                        if(t->bit1[i] != 0) {
                                f = t;
                                break;
                        }
                }
        }
        t = f;
        if(!t) {
                if(n_set < n_best) {
                        n_best = n_set;
                        fprintf(stderr, "Best answer: %d\n", n_set);
                        if(best) universe_free(best);
                        best = universe_copy(u, 0);
						
						//Added to see results in progress 
						 tile *t;
						 tile *tp;
						 int x, y;
							
						universe *bestc = universe_copy(u, 0);
						
						for(t=init->first->all_first; t; t=t->all_next) {
								tp = generation_find_tile(bestc->first, t->xpos, t->ypos, 0);
								if(!tp) continue;
								for(y=0; y<TILE_HEIGHT; y++) for(x=0; x<TILE_WIDTH; x++) {
										cellvalue v = tile_get_cell(t, x, y);
										if(v == UNKNOWN)
												tile_set_cell(tp, x, y, ON);
								}
						}

						write_life105(stdout, bestc->first);

						universe_free(bestc);
                }
                return;
        }

        // Find X position of cell
        int j;
        TILE_WORD w = t->bit1[i];
        for(j = 0; !(w & 1); j++, w = w >> 1)
                ;

        //printf("Trying %d, %d OFF\n", j, i);
        tile_set_cell(t, j, i, OFF);
        recurse(u, init);

        // try ON
        //printf("Trying %d, %d ON\n", j, i);

        if(n_set < n_best) {
                n_set++;
                tile_set_cell(t, j, i, ON);
                recurse(u, init);
                n_set--;
        }

        //printf("Done with %d, %d\n", j, i);
        tile_set_cell(t, j, i, UNKNOWN_STABLE);
}

universe *find_still_life(universe *u) {
        universe *utmp = universe_copy(u, 0);
		
        tile *t;
        tile *tp;
        int x, y;

        for(t=utmp->first->all_first; t; t=t->all_next) {
                for(y=0; y<TILE_HEIGHT; y++) for(x=0; x<TILE_WIDTH; x++) {
                        cellvalue v = tile_get_cell(t, x, y);
                        if(v == UNKNOWN)
                                tile_set_cell(t, x, y, OFF);
                }
        }

        best = NULL;
        if(!utmp->first->next)
                universe_evolve_next(utmp);
        n_set = 0;
        n_best = -1;
        recurse(utmp, u);

        if(!best) {
                fprintf(stderr, "No solution.\n");
                return NULL;
        }

        for(t=u->first->all_first; t; t=t->all_next) {
                tp = generation_find_tile(best->first, t->xpos, t->ypos, 0);
                if(!tp) continue;
                for(y=0; y<TILE_HEIGHT; y++) for(x=0; x<TILE_WIDTH; x++) {
                        cellvalue v = tile_get_cell(t, x, y);
                        if(v == UNKNOWN)
                                tile_set_cell(tp, x, y, ON);
                }
        }

        write_life105(stdout, best->first);

        universe_free(utmp);

        return best;
}
