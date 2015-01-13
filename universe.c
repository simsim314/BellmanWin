#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <assert.h>
#include "lib.h"
#include "universe.h"

universe *universe_new(cellvalue def) {
        universe *u = (universe *)allocate(sizeof *u);
        generation *g;

        u->n_gens = 1;
        u->def = def;

        g = (generation *)allocate(sizeof *g);
        g->u = u;
        g->gen = 0;

        u->first = g;
        u->last = g;

        return u;
}

void universe_free(universe *u) {
        generation *g, *gn;

        for(g = u->first; g; g = gn) {
                gn = g->next;
                free(g);
        }

        free(u);
}

static uint32_t poshash(uint32_t tx, uint32_t ty) {
        return ((2 * tx) + (3 * ty)) % HASH_SIZE;
}

tile *generation_find_tile(generation *g, int xpos, int ypos, int create) {
        int tx = xpos - (((unsigned int)xpos) % TILE_WIDTH);
        int ty = ypos - (((unsigned int)ypos) % TILE_HEIGHT);

        tile *t;
        for(t = g->xyhash[poshash(tx, ty)]; t; t = t->hashnext) {
                if((t->xpos == tx) && (t->ypos == ty))
                        return t;
        }

        if(!create) return NULL;

        t = (tile *)allocate(sizeof *t);
        t->xpos = tx;
        t->ypos = ty;
        uint32_t thash = poshash(tx, ty);
        t->hashnext = g->xyhash[thash];
        g->xyhash[thash] = t;
        g->ntiles++;

        //printf("New tile %p (%d, %d) in %d\n", t, t->xpos, t->ypos, g->gen);

        int i;
        for(i=0; i<TILE_HEIGHT; i++) {
                t->bit0[i] = (g->u->def & 1) ? ~0 : 0;
                t->bit1[i] = (g->u->def & 2) ? ~0 : 0;
        }

        tile *t2;

        t2 = generation_find_tile(g, xpos + TILE_WIDTH, ypos, 0);
        if(t2) { t2->left = t; t->right = t2; }

        t2 = generation_find_tile(g, xpos - TILE_WIDTH, ypos, 0);
        if(t2) { t2->right = t; t->left = t2; }

        t2 = generation_find_tile(g, xpos, ypos + TILE_HEIGHT, 0);
        if(t2) { t2->up = t; t->down = t2; }

        t2 = generation_find_tile(g, xpos, ypos - TILE_HEIGHT, 0);
        if(t2) { t2->down = t; t->up = t2; }

        if(g->prev) {
                t2 = generation_find_tile(g->prev, xpos, ypos, 0);
                if(t2) { t2->next = t; t->prev = t2; t->auxdata = t2->auxdata; }
        }

        if(g->next) {
                t2 = generation_find_tile(g->next, xpos, ypos, 0);
                if(t2) { t2->prev = t; t->next = t2; }
        }

        // Append to the end of the list of all tiles in this
        // generation. It's important to add to the end, because we
        // might be partway through evolving, and in this case we want
        // to process the newly added tile.
        if(g->all_last) g->all_last->all_next = t;
        else g->all_first = t;
        g->all_last = t;

        return t;
}

generation *universe_find_generation(universe *u, uint32_t gen, int create) {
        generation *g;

        assert(u);

        if(create == 0) {
                if(gen >= u->n_gens)
                        return NULL;

                for(g = u->first; g && (g->gen != gen); g = g->next);
                assert(g);
        } else {
                for(g = u->first; g->gen != gen; g = g->next) {
                        if(!g->next) {
                                g->next = (generation *)allocate(sizeof *g->next);

                                g->next->u = g->u;
                                u->last->next = g->next;
                                g->next->prev = g;
                                u->last = g->next;
                                
                                g->next->gen = g->u->n_gens;
                                g->u->n_gens++;
                        }
                }
        }

        return g;
}

tile *universe_find_tile(universe *u, 
                         uint32_t gen, uint32_t xpos, uint32_t ypos,
                         int create) {

        generation *g = universe_find_generation(u, gen, create);
        if(!g) return NULL;
        return generation_find_tile(g, xpos, ypos, create);

}

void tile_set_cell(tile *t, uint32_t xpos, uint32_t ypos, cellvalue v_) {
        xpos = xpos % TILE_WIDTH;
        ypos = ypos % TILE_HEIGHT;
        TILE_WORD v = v_;
        TILE_WORD mask = ~(((TILE_WORD)1) << xpos);
        t->bit0[ypos] = (t->bit0[ypos] & mask) | ((v & 1) << xpos);
        t->bit1[ypos] = (t->bit1[ypos] & mask) | (((v >> 1) & 1) << xpos);

        t->flags |= CHANGED;
        if(v_ != OFF)
                t->flags |= IS_LIVE;
}

cellvalue tile_get_cell(tile *t, uint32_t xpos, uint32_t ypos) {
        xpos = xpos % TILE_WIDTH;
        ypos = ypos % TILE_HEIGHT;

        return ((t->bit0[ypos] >> xpos) & 1) | (((t->bit1[ypos] >> xpos) & 1) << 1);
}

void generation_set_cell(generation *g, int x, int y, cellvalue v) {
        tile *t = generation_find_tile(g, x, y, 1);
        tile_set_cell(t, x, y, v);
}

void universe_evolve(universe *u, uint32_t gen) {
        assert(u);
        assert(gen < (u->n_gens - 1));

        generation *g;
        for(g = u->first; g && (g->gen != gen); g = g->next);
        assert(g);

        generation_evolve(g, tile_evolve_bitwise_3state);
}

void universe_evolve_next(universe *u) {
        generation *g = (generation *)allocate(sizeof *g);

        g->u = u;
        u->last->next = g;
        g->prev = u->last;
        u->last = g;

        g->gen = u->n_gens;
        u->n_gens++;

        generation_evolve(g->prev, tile_evolve_bitwise_3state);
}

void generation_evolve(generation *g, evolve_func *func) {

        assert(g->next);

        evolve_result genflags = 0;
        int n_active = 0;
        int delta_prev = 0;
        //printf("Evolve gen: %d -> %d\n", g->gen, g->gen + 1);

        tile *t;
        for(t = g->all_first; t; t = t->all_next) {

                // If this tile and all its neighbours are dead, we can skip it
#if 0 // this breaks bellman
                if((t->flags & IS_DEAD) &&
                   (!t->left || (t->left->flags & IS_DEAD)) &&
                   (!t->right || (t->right->flags & IS_DEAD)) &&
                   (!t->up || (t->up->flags & IS_DEAD)) &&
                   (!t->down || (t->down->flags & IS_DEAD)))
                        continue;
#endif

                if(!t->next) {
                        t->next = generation_find_tile(g->next, t->xpos, t->ypos, 1);
                }

                //printf("   Tile (%d, %d): %x\n", t->xpos, t->ypos, t->flags);

                // Unchanged tiles can be skipped too.
                if(!(t->flags & CHANGED)) {
                        genflags |= t->next->flags;
                        n_active += t->next->n_active;
                        delta_prev += t->next->delta_prev;
                        continue;
                }
                
                //evolve_result res = tile_evolve_simple(t, t->next);
                //evolve_result res = tile_evolve_bitwise(t, t->next);
                evolve_result res = func(t, t->next);
                t->next->flags = res;
                genflags |= res;
                n_active += t->next->n_active;
                delta_prev += t->next->delta_prev;

                // We add new blank tiles in the current generation.
                // They get added to the end of the list that we're
                // iterating over, so we process them before leaving
                // the loop.
                if(res & EXPAND_UP) {
                        t->up = generation_find_tile(g, t->xpos, t->ypos - TILE_HEIGHT, 1);
                        t->up->flags |= CHANGED;
                        //printf("EXPAND: %p\n", t->up);
                }

                if(res & EXPAND_DOWN) {
                        t->down = generation_find_tile(g, t->xpos, t->ypos + TILE_HEIGHT, 1);
                        t->down->flags |= CHANGED;
                        //printf("EXPAND: %p\n", t->down);
                }

                if(res & EXPAND_LEFT) {
                        t->left = generation_find_tile(g, t->xpos - TILE_WIDTH, t->ypos, 1);
                        t->left->flags |= CHANGED;
                        //printf("EXPAND: %p\n", t->left);
                }

                if(res & EXPAND_RIGHT) {
                        t->right = generation_find_tile(g, t->xpos + TILE_WIDTH, t->ypos, 1);
                        t->right->flags |= CHANGED;
                        //printf("EXPAND: %p\n", t->right);
                }

                t->flags &= ~CHANGED;
        }

        g->next->flags = genflags;
        g->next->n_active = n_active;
        g->next->delta_prev = delta_prev;
        g->flags &= ~CHANGED;
#if 0
        printf("Gen %d flags %x, gen %d flags %x\n",
               g->gen, g->flags,
               g->next->gen, g->next->flags);
#endif
}

void tile_find_bounds(tile *t, int *l, int *r, int *t_, int *b) {
        int ymin = TILE_HEIGHT, ymax = 0;
        int xmin = TILE_WIDTH, xmax = 0;
        int y;

        for(y=0; y<TILE_HEIGHT; y++) {
                TILE_WORD set = t->bit0[y] | t->bit1[y];
                int bit;

                if(set) {
                        if(y > ymax) ymax = y;
                        if(y < ymin) ymin = y;

                        while(set) {
                                bit = __builtin_ffsll(set) - 1;
                                if(bit > xmax) xmax = bit;
                                if(bit < xmin) xmin = bit;
                                set &= ~(((TILE_WORD)1) << bit);
                        }
                }
        }

        *l = xmin;
        *r = xmax;
        *t_ = ymin;
        *b = ymax;
}

void generation_find_bounds(generation *g, int *l, int *r, int *t_, int *b) {
        tile *t;
        int ll = INT_MAX, rr = -INT_MAX, tt = INT_MAX, bb = -INT_MAX;

        for(t = g->all_first; t; t = t->all_next) {
                if(!(t->flags & (IS_LIVE | HAS_UNKNOWN_CELLS)))
                        continue;

                if(t->xpos < ll) ll = t->xpos;
                if((t->xpos + TILE_WIDTH) > rr) rr = t->xpos + TILE_WIDTH;

                if(t->ypos < tt) tt = t->ypos;
                if((t->ypos + TILE_HEIGHT) > bb) bb = t->ypos + TILE_HEIGHT;
        }

        *l = ll; *r = rr; *t_ = tt; *b = bb;
}

universe *universe_copy(universe *from, int gen) {
        universe *u = universe_new(OFF);

        generation *g = universe_find_generation(from, gen, 0);
        tile *t;
        for(t = g->all_first; t; t = t->all_next) {
                tile *t2 = universe_find_tile(u, 0, t->xpos, t->ypos, 1);
                memcpy(&t2->bit0, &t->bit0, sizeof t->bit0);
                memcpy(&t2->bit1, &t->bit1, sizeof t->bit1);
        }

        return u;
}

#define FLAG(name) do { \
                if(flags & name) { \
                        flags &= ~name; \
                        p += sprintf(p, "%s ", #name ); \
                } \
        } while(0)

const char *flag2str(evolve_result flags) {
        static char buffer[10000];
        char *p = buffer;
        buffer[0] = '\0';

        FLAG(EXPAND_UP);
        FLAG(EXPAND_DOWN);
        FLAG(EXPAND_LEFT);
        FLAG(EXPAND_RIGHT);
        FLAG(ACTIVE);
        FLAG(ABORT);
        FLAG(HAS_UNKNOWN_SUCCESSORS);
        FLAG(HAS_ACTIVE_SUCCESSORS);
        FLAG(DIFFERS_FROM_STABLE);
        FLAG(HAS_UNKNOWN_CELLS);
        FLAG(IS_DEAD);
        FLAG(CHANGED);
        FLAG(IN_FORBIDDEN_REGION);
        FLAG(DIFFERS_FROM_PREVIOUS);
        FLAG(DIFFERS_FROM_2PREV);
        FLAG(IS_LIVE);
        FLAG(FILTER_MISMATCH);
		FLAG(HAS_ON_CELLS);

        if(flags) {
                p += sprintf(p, "%x?", flags);
        }
        return buffer;
}

void tile_set_flags(tile *t) {
        int y;

        TILE_WORD all_live = 0, all_unk = 0;

        for(y=0; y<TILE_HEIGHT; y++) {
                TILE_WORD bit0 = t->bit0[y];
                TILE_WORD bit1 = t->bit1[y];

                TILE_WORD live = bit0 & ~bit1;
                TILE_WORD unk = bit1;

                all_live |= live;
                all_unk |= unk;
        }

        t->flags &= ~(IS_DEAD | IS_LIVE | HAS_UNKNOWN_CELLS);

        if(all_live)
                t->flags |= IS_LIVE;
        else
                t->flags |= IS_DEAD;

        if(all_unk)
                t->flags |= HAS_UNKNOWN_CELLS;
}
