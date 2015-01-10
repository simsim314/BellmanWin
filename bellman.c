#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <assert.h>
#include "universe.h"
#include "readwrite.h"
#include "bitwise.h"

#define RECURSE(x) //printf x
#define PRUNE(x) //printf x
#define CHECK(x) //printf x
#define YES 1
#define NO 0

static universe *u_static, *u_evolving, *u_forbidden, *u_filter;

// Search space restriction parameters:
static unsigned int first_encounter_gen = 2;
static unsigned int last_encounter_gen = 20;
static unsigned int repair_interval = 20;
static unsigned int stable_interval = 5;
static unsigned int max_live = 10;
static unsigned int max_active = 10;

// Symmetry constraints
static enum {
        NONE, HORIZ, VERT
} symmetry_type = NONE;
static unsigned int symmetry_ofs = 0;

// Other global values
static unsigned int max_gens;
static unsigned int n_live;

static int last_print_time = 0;

// Prune counters
static uint64_t prune_unstable;
static uint64_t prune_solution;
static uint64_t prune_encounter_too_early;
static uint64_t prune_encounter_too_late;
static uint64_t prune_stabilises_too_slowly;
static uint64_t prune_too_many_active;
static uint64_t prune_too_many_live;
static uint64_t prune_forbidden;
static uint64_t prune_filter;
static uint64_t prune_oldtotal;

#define PRINT_PRUNE(fmt, count) do {                            \
                printf("   " fmt ": %lld\n", (long long)count); \
                prune_total += count;                           \
        } while(0)

static void print_prune_counters(void) {
        uint64_t prune_total = 0;

        printf("Reasons why search space was pruned:\n");
        PRINT_PRUNE("Catalyst is unstable", prune_unstable);
        PRINT_PRUNE("Found a solution", prune_solution);
        PRINT_PRUNE("First encounter too early (first-encounter)", prune_encounter_too_early);
        PRINT_PRUNE("First encounter too late (last-encounter)", prune_encounter_too_late);
        PRINT_PRUNE("Took too long to stabilise (repair-interval)", prune_stabilises_too_slowly);
        PRINT_PRUNE("Too many active cells (max-active)", prune_too_many_active);
        PRINT_PRUNE("Too many live cells (max-live)", prune_too_many_live);
        PRINT_PRUNE("Hit forbidden region", prune_forbidden);
        PRINT_PRUNE("Filter mismatch", prune_filter);

        double prune_rate = prune_total - prune_oldtotal;
        prune_oldtotal = prune_total;

        prune_rate = prune_rate / 10000.0;

        printf("   Total: %lld (%f Kprunes/sec)\n", (long long)prune_total, prune_rate);        
}



static void read_cb(void *u_, char area, int gen, int x, int y, char c) {
        cellvalue vs = OFF, ve = OFF, vf = OFF;

        (void)u_;

        if((area == 'P') && (gen == 0)) {
                switch(c) {
                case '.': break;
                case '*': vs = ve = ON; break;
                case '@': ve = ON; break;
                case '?': vs = ve = UNKNOWN_STABLE; break;
                case '!': vs = ve = UNKNOWN_STABLE; vf = ON; break;
                }

                generation_set_cell(u_static->first, x, y, vs);
                generation_set_cell(u_evolving->first, x, y, ve);        
                generation_set_cell(u_forbidden->first, x, y, vf);
        } else if(area == 'F') {
                generation *g = universe_find_generation(u_filter, gen, 1);
				
				switch(c)
				{
				   case '*' :
					  generation_set_cell(g, x, y, ON);
					  break;
				   case '.' :
				   case ' ' :
					  generation_set_cell(g, x, y, OFF);
					  break;
				}
        }
}

static void read_param_cb(void *u_, const char *param, const char *value) {
        (void)u_;

        if(!strcmp(param, "first-encounter"))
                first_encounter_gen = strtoul(value, NULL, 10);

        else if(!strcmp(param, "last-encounter"))
                last_encounter_gen = strtoul(value, NULL, 10);

        else if(!strcmp(param, "repair-interval"))
                repair_interval = strtoul(value, NULL, 10);

        else if(!strcmp(param, "stable-interval"))
                stable_interval = strtoul(value, NULL, 10);

        else if(!strcmp(param, "max-live"))
                max_live = strtoul(value, NULL, 10);

        else if(!strcmp(param, "max-active"))
                max_active = strtoul(value, NULL, 10);

        else if(!strcmp(param, "symmetry")) {
                char typebuff[20];
                unsigned int coord;
                if(sscanf(value, "%d:%s", &coord, typebuff) != 2) {
                        fprintf(stderr, "Bad symmetry parameter: '%s'\n", value);
                        exit(-1);
                }

                if(!strcmp(typebuff, "horiz-odd")) {
                        symmetry_type = HORIZ;
                        symmetry_ofs = (coord * 2);

                } else if(!strcmp(typebuff, "horiz-even")) {
                        symmetry_type = HORIZ;
                        symmetry_ofs = (coord * 2) + 1;

                } else if(!strcmp(typebuff, "vert-odd")) {
                        symmetry_type = VERT;
                        symmetry_ofs = (coord * 2);

                } else if(!strcmp(typebuff, "vert-even")) {
                        symmetry_type = VERT;
                        symmetry_ofs = (coord * 2) + 1;
                }

                        
        }

        else fprintf(stderr, "Bad parameter: '%s' (%s)\n",
                     param, value);
}

static evolve_result bellman_evolve(tile *t, tile *out) {

        // Our evolution function is based on the 3 state Life variant.
        out->flags = tile_evolve_bitwise_3state(t, out) | CHANGED;

        // But we do another pass to (a) stop the UNKNOWN_STABLE area
        // from growing and (b) check for boundary condition
        // violations.

        tile *stable = (tile *)t->auxdata;
        if(!stable) return out->flags;
        tile *forbidden = (tile *)stable->auxdata;
        tile *filter = t->filter;
        tile *prev = t->prev;

        int y;

        TILE_WORD ul_bit0, u_bit0, ur_bit0;
        TILE_WORD ul_bit1, u_bit1, ur_bit1;
        TILE_WORD ul_bit0s, u_bit0s, ur_bit0s;
        TILE_WORD ul_bit1s, u_bit1s, ur_bit1s;       

        tile *t_up = t->up;


        if(t_up) {
                GET3WORDS(ul_bit0, u_bit0, ur_bit0, t_up, 0, TILE_HEIGHT-1);
                GET3WORDS(ul_bit1, u_bit1, ur_bit1, t_up, 1, TILE_HEIGHT-1);
        } else {
                ul_bit0 = u_bit0 = ur_bit0 = 0;
                ul_bit1 = u_bit1 = ur_bit1 = 0;
        }

        t_up = stable->up;
        if(t_up) {
                GET3WORDS(ul_bit0s, u_bit0s, ur_bit0s, t_up, 0, TILE_HEIGHT-1);
                GET3WORDS(ul_bit1s, u_bit1s, ur_bit1s, t_up, 1, TILE_HEIGHT-1);
                
        } else {
                ul_bit0s = u_bit0s = ur_bit0s = 0;
                ul_bit1s = u_bit1s = ur_bit1s = 0;
        }


        TILE_WORD l_bit0, bit0, r_bit0;
        TILE_WORD l_bit1, bit1, r_bit1;
        TILE_WORD l_bit0s, bit0s, r_bit0s;
        TILE_WORD l_bit1s, bit1s, r_bit1s;

        GET3WORDS(l_bit0, bit0, r_bit0, t, 0, 0);
        GET3WORDS(l_bit1, bit1, r_bit1, t, 1, 0);
        GET3WORDS(l_bit0s, bit0s, r_bit0s, stable, 0, 0);
        GET3WORDS(l_bit1s, bit1s, r_bit1s, stable, 1, 0);

        TILE_WORD dl_bit0, d_bit0, dr_bit0;
        TILE_WORD dl_bit1, d_bit1, dr_bit1;
        TILE_WORD dl_bit0s, d_bit0s, dr_bit0s;
        TILE_WORD dl_bit1s, d_bit1s, dr_bit1s;

        TILE_WORD interaction = 0, activity = 0, unk_succ = 0, delta_from_stable_count = 0;
        TILE_WORD delta_from_previous_count = 0;
        TILE_WORD forbid = 0;
        TILE_WORD activity2 = 0, live = 0;
        TILE_WORD filter_diff_all = 0;

        for(y=0; y<TILE_HEIGHT; y++) {
                if(y == TILE_HEIGHT-1) {
                        if(t->down) {
                                GET3WORDS(dl_bit0, d_bit0, dr_bit0, t->down, 0, 0);
                                GET3WORDS(dl_bit1, d_bit1, dr_bit1, t->down, 1, 0);
                        } else {
                                dl_bit0 = d_bit0 = dr_bit0 = 0;
                                dl_bit1 = d_bit1 = dr_bit1 = 0;
                        }
                        if(stable->down) {
                                GET3WORDS(dl_bit0s, d_bit0s, dr_bit0s, stable->down, 0, 0);
                                GET3WORDS(dl_bit1s, d_bit1s, dr_bit1s, stable->down, 1, 0);
                        } else {
                                dl_bit0s = d_bit0s = dr_bit0s = 0;
                                dl_bit1s = d_bit1s = dr_bit1s = 0;
                        }
                } else {
                        GET3WORDS(dl_bit0, d_bit0, dr_bit0, t, 0, y+1);
                        GET3WORDS(dl_bit1, d_bit1, dr_bit1, t, 1, y+1);
                        GET3WORDS(dl_bit0s, d_bit0s, dr_bit0s, stable, 0, y+1);
                        GET3WORDS(dl_bit1s, d_bit1s, dr_bit1s, stable, 1, y+1);
                }

                // Any neighbourhood which is identical to the stable
                // universe should remain stable.

                TILE_WORD stable_diff_above = 0;
                stable_diff_above |= (ul_bit0s ^ ul_bit0);
                stable_diff_above |= (ul_bit1s ^ ul_bit1);
                stable_diff_above |= (u_bit0s ^ u_bit0);
                stable_diff_above |= (u_bit1s ^ u_bit1);
                stable_diff_above |= (ur_bit0s ^ ur_bit0);
                stable_diff_above |= (ur_bit1s ^ ur_bit1);

                TILE_WORD stable_diff_mid = 0;
                stable_diff_mid |= (l_bit0s ^ l_bit0);
                stable_diff_mid |= (l_bit1s ^ l_bit1);
                stable_diff_mid |= (bit0s ^ bit0);
                stable_diff_mid |= (bit1s ^ bit1);
                stable_diff_mid |= (r_bit0s ^ r_bit0);
                stable_diff_mid |= (r_bit1s ^ r_bit1);

                TILE_WORD stable_diff_below = 0;
                stable_diff_below |= (dl_bit0s ^ dl_bit0);
                stable_diff_below |= (dl_bit1s ^ dl_bit1);
                stable_diff_below |= (d_bit0s ^ d_bit0);
                stable_diff_below |= (d_bit1s ^ d_bit1);
                stable_diff_below |= (dr_bit0s ^ dr_bit0);
                stable_diff_below |= (dr_bit1s ^ dr_bit1);

                TILE_WORD diff_mask = stable_diff_above | stable_diff_mid | stable_diff_below;

                out->bit0[y] = (out->bit0[y] & diff_mask) | (stable->bit0[y] & ~diff_mask);
                out->bit1[y] = (out->bit1[y] & diff_mask) | (stable->bit1[y] & ~diff_mask);

                // Generate a mask representing anything that's set in
                // the stable region.
                TILE_WORD stable_set_above = 0;
                stable_set_above |= (ul_bit0s & ~ul_bit1s);
                stable_set_above |= (u_bit0s & ~u_bit1s);
                stable_set_above |= (ur_bit0s & ~ur_bit1s);

                TILE_WORD stable_set_mid = 0;
                stable_set_mid |= (l_bit0s & ~l_bit1s);
                stable_set_mid |= (bit0s & ~bit1s);
                stable_set_mid |= (r_bit0s & ~r_bit1s);

                TILE_WORD stable_set_below = 0;
                stable_set_below |= (dl_bit0s & ~dl_bit1s);
                stable_set_below |= (d_bit0s & ~d_bit1s);
                stable_set_below |= (dr_bit0s & ~dr_bit1s);

                TILE_WORD set_mask = stable_set_above | stable_set_mid | stable_set_below;

                // Look for places where the output differs from the
                // stable input
                TILE_WORD was0now1 = (~bit0s & ~bit1s) & (out->bit0[y] & ~out->bit1[y]);
                TILE_WORD was1now0 = (bit0s & ~bit1s) & (~out->bit0[y] & ~out->bit1[y]);

                TILE_WORD delta_from_stable = was0now1 | was1now0;

                live |= delta_from_stable;
                delta_from_stable &= set_mask;
                interaction |= delta_from_stable;

                // Have any forbidden cells changed?
                if(forbidden)
                        forbid |= forbidden->bit0[y] & (was0now1 | was1now0);

                // Also count the number of cells which differ from
                // the stable input. 4 rounds of the bitwise bit
                // counting algorithm gets us to 16 bit subtotals
                // which we accumulate; we finish off the addition
                // outside the loop.

                // With a careful choice of tile size it should be
                // possible to move the last round out of the loop
                // too.

                delta_from_stable = (delta_from_stable & 0x5555555555555555) + ((delta_from_stable >> 1) & 0x5555555555555555);
                delta_from_stable = (delta_from_stable & 0x3333333333333333) + ((delta_from_stable >> 2) & 0x3333333333333333);
                delta_from_stable = (delta_from_stable & 0x0f0f0f0f0f0f0f0f) + ((delta_from_stable >> 4) & 0x0f0f0f0f0f0f0f0f);
                delta_from_stable = (delta_from_stable & 0x00ff00ff00ff00ff) + ((delta_from_stable >> 8) & 0x00ff00ff00ff00ff);

                delta_from_stable_count += delta_from_stable;

                // Look for places where the universe is changing
                was0now1 = (~bit0 & ~bit1) & (out->bit0[y] & ~out->bit1[y]);
                was1now0 = (bit0 & ~bit1) & (~out->bit0[y] & ~out->bit1[y]);
                TILE_WORD delta_from_previous = (was0now1 | was1now0);

                activity |= delta_from_previous;

                delta_from_previous &= set_mask;

                delta_from_previous = (delta_from_previous & 0x5555555555555555) + ((delta_from_previous >> 1) & 0x5555555555555555);
                delta_from_previous = (delta_from_previous & 0x3333333333333333) + ((delta_from_previous >> 2) & 0x3333333333333333);
                delta_from_previous = (delta_from_previous & 0x0f0f0f0f0f0f0f0f) + ((delta_from_previous >> 4) & 0x0f0f0f0f0f0f0f0f);
                delta_from_previous = (delta_from_previous & 0x00ff00ff00ff00ff) + ((delta_from_previous >> 8) & 0x00ff00ff00ff00ff);

                delta_from_previous_count += delta_from_previous;

                if(prev) {
                        was0now1 = (~prev->bit0[y] & ~prev->bit1[y]) & (out->bit0[y] & ~out->bit1[y]);
                        was1now0 = (prev->bit0[y] & ~prev->bit1[y]) & (~out->bit0[y] & ~out->bit1[y]);
                        TILE_WORD delta_from_2prev = (was0now1 | was1now0);

                        activity2 |= delta_from_2prev;

                }

                // Look for unknown successors
                unk_succ |= (out->bit1[y] & ~out->bit0[y]);

                // Compare against user-specified filter pattern
                TILE_WORD filter_bit0 = filter ? filter->bit0[y] : 0;
                TILE_WORD filter_bit1 = filter ? filter->bit1[y] : (TILE_WORD)~0;

                TILE_WORD filter_diff = out->bit0[y] ^ filter_bit0;
                filter_diff &= ~(filter_bit1 | out->bit1[y]);
                filter_diff_all |= filter_diff;
#if 0
                if(filter_bit1 != ~0) {
                        printf("f%d: %16llx/%16llx\n", y, filter_bit0 & ~filter_bit1, filter_bit1);
                        printf("o%d: %16llx/%16llx\n", y, out->bit0[y] & ~filter_bit1, out->bit1[y] & ~filter_bit1);
                        printf("d%d: %16llx\n", y, filter_diff);
                }
#endif
#if 0
                int x;
                for(x=0; x<TILE_WIDTH; x++) {
                        int cb0 = (neigh_total0 >> x) & 1;
                        int cb1 = (neigh_total1 >> x) & 1;
                        int cb2 = (neigh_total2 >> x) & 1;
                        int cb3 = (neigh_total3 >> x) & 1;
                        int ub0 = (neigh_unk_total0 >> x) & 1;
                        int ub1 = (neigh_unk_total1 >> x) & 1;
                        int ub2 = (neigh_unk_total2 >> x) & 1;
                        int ub3 = (neigh_unk_total3 >> x) & 1;
                        int v = (mid >> x) & 1;
                        v += ((mid_unk >> x) & 1) << 1;
                        int nv = (is_live >> x) & 1;
                        nv += ((is_unk >> x) & 1) << 1;
                        printf("%d, %d: v=%d, count=%d, unk=%d, new=%d, abort %x\n",
                               x, y, v, (cb3 * 8) + (cb2 * 4) + (cb1 * 2) + cb0,
                               (ub3 * 8) + (ub2 * 4) + (ub1 * 2) + ub0, nv, abort);

                }
#endif

                // Shift the previous results
                ul_bit0 = l_bit0; u_bit0 = bit0; ur_bit0 = r_bit0;
                ul_bit1 = l_bit1; u_bit1 = bit1; ur_bit1 = r_bit1;

                l_bit0 = dl_bit0; bit0 = d_bit0; r_bit0 = dr_bit0;
                l_bit1 = dl_bit1; bit1 = d_bit1; r_bit1 = dr_bit1;

                ul_bit0s = l_bit0s; u_bit0s = bit0s; ur_bit0s = r_bit0s;
                ul_bit1s = l_bit1s; u_bit1s = bit1s; ur_bit1s = r_bit1s;

                l_bit0s = dl_bit0s; bit0s = d_bit0s; r_bit0s = dr_bit0s;
                l_bit1s = dl_bit1s; bit1s = d_bit1s; r_bit1s = dr_bit1s;

        }

        // The delta_from_stable and delta_from_previous counters are
        // still split into 16 bit subtotals; finish them off here

        delta_from_stable_count = (delta_from_stable_count & 0x0000ffff0000ffff) + ((delta_from_stable_count >> 16) & 0x0000ffff0000ffff);
        delta_from_stable_count = (delta_from_stable_count & 0x00000000ffffffff) + ((delta_from_stable_count >> 32) & 0x00000000ffffffff);

        delta_from_previous_count = (delta_from_previous_count & 0x0000ffff0000ffff) + ((delta_from_previous_count >> 16) & 0x0000ffff0000ffff);
        delta_from_previous_count = (delta_from_previous_count & 0x00000000ffffffff) + ((delta_from_previous_count >> 32) & 0x00000000ffffffff);

        out->n_active = delta_from_stable_count;
        out->delta_prev = delta_from_previous_count;

        if(interaction != 0) out->flags |= DIFFERS_FROM_STABLE;
        if(unk_succ != 0) out->flags |= HAS_UNKNOWN_CELLS;
        if(forbid != 0) out->flags |= IN_FORBIDDEN_REGION;
        if(activity != 0) out->flags |= DIFFERS_FROM_PREVIOUS;
        if((activity2 != 0) || !prev) out->flags |= DIFFERS_FROM_2PREV;
        if(live != 0) out->flags |= IS_LIVE;
        if(filter_diff_all != 0) out->flags |= FILTER_MISMATCH;

        return out->flags;
}

static generation *bellman_evolve_generations(generation *g, unsigned int end) {
        tile *t;
        g->flags |= CHANGED;

        for(t = g->all_first; t; t = t->all_next)
                t->flags |= CHANGED;

        while(g->gen < end) {
                //printf("Evolving: %d\n", g->gen);
                generation_evolve(g, bellman_evolve);
                g = g->next;
        }
        return g->prev;
}

static int dumpcount = 0;

static void dump(int full, unsigned int gen_nr) {

        char name[30];
        unsigned int i;

        printf("Dumping %d\n", dumpcount);

        if(full) {
                for(i=0; i<max_gens; i++) {
                        printf("   %03d: %s\n", i, 
                               flag2str(universe_find_generation(u_evolving, i, 0)->flags));
                        
                }
        }
        dumpcount++;
}

static int solcount = 0;

static void bellman_found_solution(const char *type, unsigned int gens) {
        solcount++;
        printf("Found solution %d type %s, gens %d\n", solcount, type, gens);

        char name[30];

        unsigned int i;
		tile *t;
		
        snprintf(name, sizeof name, "result%06d-%s.out", solcount, type);
        FILE *f = fopen(name, "w");
        if(f) {
                fprintf(f, "#S first-encounter %d\n", first_encounter_gen);
                fprintf(f, "#S last-encounter %d\n", last_encounter_gen);
                fprintf(f, "#S repair-interval %d\n", repair_interval);
                fprintf(f, "#S stable-interval %d\n", stable_interval);
                fprintf(f, "#S max-live %d\n", max_live);
                fprintf(f, "#S max-active %d\n", max_active);


                for(t = u_static->first->all_first; t; t = t->all_next) {
                        tile *t2 = universe_find_tile(u_evolving, 0, t->xpos, t->ypos, 0);
                        fprintf(f, "#P %d %d\n", t->xpos, t->ypos);

                        int x, y;

                        for(y=0; y<TILE_HEIGHT; y++) {
                                for(x=0; x<TILE_WIDTH; x++) {
                                        char c = '.';
                                        
                                        if(t2 && tile_get_cell(t2, x, y) == ON)
                                                c = '@';

                                        if(tile_get_cell(t, x, y) == ON)
                                                c = '*';
                                        else if(tile_get_cell(t, x, y) != OFF)
                                                c = '?';

                                        fputc(c, f);
                                }
                                fputc('\n', f);
                        }
                }


                fclose(f);
        } else perror(name);
}

static void bellman_choose_cells(universe *u, generation *g);

static void bellman_recurse(universe *u, generation *g) {

        int t_now = time(NULL);
        if((t_now - last_print_time) > 10) {
                last_print_time = t_now;
                print_prune_counters();
        }

        // First make sure the static pattern is truly static

        // TODO: check only the neighbourhood of the cell we just
        // modified!

        tile *t;
        for(t = u_static->first->all_first; t; t = t->all_next) {
                evolve_result res = tile_stabilise_3state(t, t->next);
           
                if(res & ABORT) {
                        PRUNE(("Stable world is unstable\n"));
                        prune_unstable++;
                        return;
                }
        }

        // Now check that the evolving universe is behaving itself
        generation *ge;
        evolve_result all_gens = 0;
        int first_active_gen = 0;
        int changed = 0;
		int stabilized = NO; 
		
        for(ge = u->first; ge && ge->next; ge = ge->next) {
                if(ge->flags & CHANGED) {
                        changed = 1;
                        ge->flags &= ~CHANGED;
                }

                if(changed) {
                        CHECK(("Evolving generation %d\n", ge->next->gen));
                        generation_evolve(ge, bellman_evolve);
                }
                all_gens |= ge->flags;

                if((first_active_gen == 0) && (ge->flags & DIFFERS_FROM_STABLE))
                        first_active_gen = ge->gen;
				
				//stabilized flag is set to handle secondary destabilization.
				if(first_active_gen > 0 && ge->n_active == 0)
					stabilized = YES;
				
				//If stabilized and destabilized again, before reaching stable_interval,
				//then first_active_gen will be set to the new active generation. 
				if((stabilized == YES) && (ge->flags & DIFFERS_FROM_STABLE))
                {
					first_active_gen = ge->gen;
					stabilized = NO;
				}
				
                CHECK(("Checking generation %d, flags %x all %x first_active_gen %d n_active %d changed %d\n", 
                       ge->gen, ge->flags, all_gens, first_active_gen, ge->n_active, changed));

                if(ge->flags & FILTER_MISMATCH) {
                        PRUNE(("Didn't match filter\n"));
                        //dump(1, 0);
                        prune_filter++;
                        return;
                }

                if(ge->flags & IN_FORBIDDEN_REGION) {
                        PRUNE(("Hit forbidden region\n"));
                        prune_forbidden++;
                        return;
                }

                // No activity is allowed prior to a point
                if(ge->gen < first_encounter_gen) {
                        if(ge->flags & DIFFERS_FROM_STABLE) {
                                PRUNE(("Activity before generation %d\n", first_encounter_gen));
                                prune_encounter_too_early++;
                                return;                        
                        }
                }
                
                if(last_encounter_gen && (ge->gen >= last_encounter_gen)) {
                        if(!(all_gens & DIFFERS_FROM_STABLE) && !(ge->flags & HAS_UNKNOWN_CELLS)) {
                                PRUNE(("No activity before generation %d\n", last_encounter_gen));
                                prune_encounter_too_late++;
                                return;                        
                        }
                }

                if((first_active_gen > 0) && (ge->gen > first_active_gen + repair_interval)) {
                        // We reached the end of the repair interval;
                        // now the pattern must remain stable

                        if(ge->n_active > 0) {
                                PRUNE(("Activity after generation %d\n", first_active_gen + repair_interval));
                                prune_stabilises_too_slowly++;
                                return;
                        }
                }

                if((first_active_gen > 0) && (ge->gen > first_active_gen + repair_interval + stable_interval) && (ge->gen >= (u_filter->n_gens - 1))) {
                        // We reached the end of the stable interval;
                        // we may have a solution

                        if(!(ge->flags & HAS_UNKNOWN_CELLS)) {
                                bellman_found_solution("4", ge->gen);
                                //dump(1, 0);
                                PRUNE(("found a solution\n"));
                                prune_solution++;
                                return;
                        } else {
                                break;
                        }
                }

                if(ge->n_active > max_active) {
                        PRUNE(("Too much activity at generation %d\n", ge->gen));
                        prune_too_many_active++;
                        return;
                }
#if 0
                if((ge->gen == first_activity) && !(all_gens & DIFFERS_FROM_STABLE) && !(ge->flags & HAS_UNKNOWN_CELLS)) {
                        PRUNE(("No activity at generation %d\n", first_activity));
                        //dump(1, 0);
                        return;
                }
#endif
                //printf("First activity %d, checking %d\n", first_activity, ge->gen);
#if 0
                if(ge->gen >= first_activity + repair_interval - 1) {
                        if(ge->flags & HAS_ACTIVE_SUCCESSORS) {
                                PRUNE(("Activity after generation %d\n", first_activity + repair_interval));
                                        return;
                        }
                }
#endif
        }
        //dump(1, 0);

        bellman_choose_cells(u, g);
}

#define TRY(cdx, cdy)                                                   \
        if(tile_get_cell(t->prev, x + cdx, y + cdy) == UNKNOWN_STABLE && validate_xy_for_symmetry(x + cdx, y + cdy) == YES) { \
                dx = cdx;                                               \
                dy = cdy;                                               \
                goto found;                                             \
        }

static int validate_xy_for_symmetry(int x, int y)
{
	switch(symmetry_type) {
	case NONE:
		   return YES;
	case HORIZ:
			if(y >= symmetry_ofs - y)
				return YES;
			else
				return NO;
			
	case VERT:
			if(x >= symmetry_ofs - x)
				return YES;
			else
				return NO;
	}
}

static void bellman_choose_cells(universe *u, generation *g) {
        // Look for a tile with some unknown cells.

        g = u_evolving->first;
        
        tile *t;
        do {
                for(t = g->all_first; 
                    t && !(t->flags & HAS_UNKNOWN_CELLS);
                    t = t->all_next)
                        ;
                if(!t) g = g->next;
        } while(g && !t);

        if(!g) {
                // We got all the way to the end of the pattern.
                bellman_found_solution("1", max_gens);
                //dump(1, 0);
                PRUNE(("found a solution\n"));
                prune_solution++;
                return;
        }

        // Find an unknown successor cell that's in the neighbourhood
        // of an unknown-stable predecessor cell.

        assert(t->prev);
        //generation_evolve(g, bellman_evolve);
        //printf("Generation %d has unknown cells\n", g->gen);
        int x, y, dx = 2, dy = 2;

        // Look for direct predecessors first ...

        for(y=0; y<TILE_HEIGHT; y++) {
                TILE_WORD is_unk = 0;
                is_unk = t->bit1[y] & ~t->bit0[y];
                if(is_unk) {
                        for(x = 0; x < TILE_WIDTH; x++) {
                                	
								if((is_unk >> x) & 1) {
                                        assert(tile_get_cell(t, x, y) == UNKNOWN);
                                        // Now look for an unknown-stable cell near it.
                                        if((x == 0) || (x == TILE_WIDTH-1) || (y == 0) || (y == TILE_HEIGHT-1)) {
                                                printf("TODO: handle tile wrap! (%d, %d, %d)\n", g->gen, x, y);
                                                dump(1, 0);
                                                assert(0);
                                        }

                                        
                                        TRY(0, 0);
                                }
                        }

                }
        }

        // ... then orthogonally adjacent cells ...

        for(y=0; y<TILE_HEIGHT; y++) {
                TILE_WORD is_unk = 0;
                is_unk = t->bit1[y] & ~t->bit0[y];
                if(is_unk) {
                        for(x = 0; x < TILE_WIDTH; x++) {
								
                                if((is_unk >> x) & 1) {
                                        assert(tile_get_cell(t, x, y) == UNKNOWN);
                                        // Now look for an unknown-stable cell near it.
                                        if((x == 0) || (x == TILE_WIDTH-1) || (y == 0) || (y == TILE_HEIGHT-1)) {
                                                printf("TODO: handle tile wrap! (%d, %d, %d)\n", g->gen, x, y);
                                                dump(1, 0);
                                                assert(0);
                                        }

                                        
                                        TRY(1, 0);
                                        TRY(0, 1);
                                        TRY(-1, 0);
                                        TRY(0, -1);
                                }
                        }

                }
        }

        // ... then diagonally adjacent ones.

        for(y=0; y<TILE_HEIGHT; y++) {
                TILE_WORD is_unk = 0;
                is_unk = t->bit1[y] & ~t->bit0[y];
                if(is_unk) {
                        for(x = 0; x < TILE_WIDTH; x++) {
								
                                if((is_unk >> x) & 1) {
                                        assert(tile_get_cell(t, x, y) == UNKNOWN);
                                        // Now look for an unknown-stable cell near it.
                                        if((x == 0) || (x == TILE_WIDTH-1) || (y == 0) || (y == TILE_HEIGHT-1)) {
                                                printf("TODO: handle tile wrap! (%d, %d, %d)\n", g->gen, x, y);
                                                dump(1, 0);
                                                assert(0);
                                        }

                                        
                                        TRY(-1, -1);
                                        TRY(-1, 1);
                                        TRY(1, -1);
                                        TRY(1, 1);
                                }
                        }

                }
        }

        printf("Didn't find an unknown cell!\n");
        dump(1, 0);
        assert(0);
        return;

found:
	assert(tile_get_cell(t, x, y) == UNKNOWN);
	assert(tile_get_cell(t->prev, x+dx, y+dy) == UNKNOWN_STABLE);
	assert(tile_get_cell((tile *)t->auxdata, x+dx, y+dy) == UNKNOWN_STABLE);
	
	RECURSE(("Generation %d, unknown cell at (%d, %d, %d)\n",
			 g->gen, g->gen + 1, x+dx, y+dy));
	assert(dx <= 1);
	assert(dy <= 1);
	x += dx;
	y += dy;

		int xmirror, ymirror;
		
		switch(symmetry_type) {
		case NONE:
				xmirror = x;
				ymirror = y;
				break;

		case HORIZ:
				xmirror = x;
				ymirror = symmetry_ofs - y;
				break;

		case VERT:
				xmirror = symmetry_ofs - x;
				ymirror = y;
				break;

		default: assert(0);
		}

		if(tile_get_cell(t->prev, xmirror, ymirror) != UNKNOWN_STABLE) {
				fprintf(stderr, "Input region is asymmetric (%d,%d)=%d (%d,%d)=%d\n",
						x, y, tile_get_cell(t->prev, x, y),
						xmirror, ymirror, tile_get_cell(t->prev, xmirror, ymirror));
				exit(-1);
		}

#if 0
		tile_set_cell(t->prev, x, y, OFF);
		tile_set_cell(t->auxdata, x, y, OFF);
		g->prev->flags |= CHANGED;

		RECURSE(("Recursing with (%d,%d) = OFF\n", x, y));
		bellman_recurse(u, g->prev);
#endif
		int dn = 1;
		
		//Symmetry many times turn ON 2 cells
		if(xmirror != x || ymirror != y)
			dn++;
		
		if(n_live + dn <= max_live) {
				tile_set_cell(t->prev, x, y, ON);
				tile_set_cell((tile *)t->auxdata, x, y, ON);
				tile_set_cell(t->prev, xmirror, ymirror, ON);
				tile_set_cell((tile *)t->auxdata, xmirror, ymirror, ON);
				g->prev->flags |= CHANGED;
				RECURSE(("Recursing with (%d,%d) = ON\n", x, y));
					
				n_live+=dn;
					bellman_recurse(u, g->prev);
				n_live-=dn;
		} else { 
				PRUNE(("Too many live cells\n"));
				prune_too_many_live++;
		}
#if 1
		tile_set_cell(t->prev, x, y, OFF);
		tile_set_cell((tile *)t->auxdata, x, y, OFF);
		tile_set_cell(t->prev, xmirror, ymirror, OFF);
		tile_set_cell((tile *)t->auxdata, xmirror, ymirror, OFF);
		g->prev->flags |= CHANGED;

		RECURSE(("Recursing with (%d,%d) = OFF\n", x, y));
		
		
		bellman_recurse(u, g->prev);
#endif
	
	
		tile_set_cell(t->prev, x, y, UNKNOWN_STABLE);
		tile_set_cell((tile *)t->auxdata, x, y, UNKNOWN_STABLE);
		tile_set_cell(t->prev, xmirror, ymirror, UNKNOWN_STABLE);
		tile_set_cell((tile *)t->auxdata, xmirror, ymirror, UNKNOWN_STABLE);
		g->prev->flags |= CHANGED;
		
	
		
}

int main(int argc, char *argv[]) {

        enum {
                SEARCH,
                CLASSIFY
        } mode = SEARCH;
        int verbose = 0;

        u_static = universe_new(OFF);
        u_evolving = universe_new(OFF);
        u_forbidden = universe_new(OFF);
        u_filter = universe_new(UNKNOWN);

        int c;

        while((c = getopt(argc, argv, "cv")) != -1) switch(c) {
        case 'c':
                mode = CLASSIFY;
                break;

        case 'v': verbose++; break;
        }

        FILE *f = fopen(argv[optind], "r");
        if(!f) {
                perror(argv[optind]);
                return -1;
        }

        read_life105(f, read_cb, read_param_cb, NULL);

        fclose(f);

        max_gens = last_encounter_gen + repair_interval + stable_interval + 2;
		
		if(max_gens < (u_filter->n_gens + 1))
		{
			max_gens = u_filter->n_gens + 1;
		}
		
        n_live = 0;

        universe_evolve_next(u_static);

        unsigned int i;
        int x, y;
        generation *g;
        tile *t, *tp;

        g = universe_find_generation(u_static, 0, 0);
        for(t = g->all_first; t; t = t->all_next) {
                tile *t2 = universe_find_tile(u_forbidden, 0, t->xpos, t->ypos, 0);
                if(t2) t->auxdata = t2;
        }

        for(i=0; i<max_gens; i++) {
                universe_evolve_next(u_evolving);

                g = universe_find_generation(u_evolving, i, 0);
                for(t = g->all_first; t; t = t->all_next) {
                        tile *t2 = universe_find_tile(u_static, 0, t->xpos, t->ypos, 0);
                        if(t2) t->auxdata = t2;

                        t2 = universe_find_tile(u_filter, g->gen + 1, t->xpos, t->ypos, 0);
                        if(t2) t->filter = t2;
                }
        }

        /* set auxdata in the final generation: */
        g = universe_find_generation(u_evolving, i, 0);
        for(t = g->all_first; t; t = t->all_next) {
                tile *t2 = universe_find_tile(u_static, 0, t->xpos, t->ypos, 0);
                if(t2) t->auxdata = t2;
                t->filter = NULL;
        }

        bellman_evolve_generations(u_evolving->first, max_gens);

        int ac_first, ac_last;
        uint32_t klass;

        switch(mode) {
        case SEARCH:
                printf("max_gens: %d\n", max_gens);
                bellman_choose_cells(u_evolving, u_evolving->first);

                print_prune_counters();
                break;

        case CLASSIFY:

                if(verbose > 0) {
                        if(verbose > 1)
                                dump(1, 0);
                        for(g = u_evolving->first; g; g = g->next) {
                                printf("Generation %d: %x: %s\n", g->gen, g->flags, flag2str(g->flags));
                        }
                        
                }

                // print the history
                int in_interaction = 0, interaction_nr = 0;

                for(g = u_evolving->first->next; g; g = g->next) {
                        if(!(g->flags & IS_LIVE)) {
                                printf("log: g%d: died out\n", g->gen);
                                break;
                        }
                        if(g->flags & HAS_UNKNOWN_CELLS) {
                                printf("log: g%d: became undetermined\n", g->gen);
                                break;
                        }
                        if(!(g->flags & DIFFERS_FROM_PREVIOUS)) {
                                printf("log: g%d: became stable\n", g->gen);
                                break;
                        }
                        if(!(g->flags & DIFFERS_FROM_2PREV)) {
                                printf("log: g%d: became period 2\n", g->gen);
                                break;
                        }

                        if(!in_interaction) {
                                if(g->flags & DIFFERS_FROM_STABLE) {
                                        interaction_nr++;
                                        in_interaction = 1;
                                        printf("log: g%d: interaction %d begins\n", 
                                               g->gen, interaction_nr);
                                }
                        } else {
                                if(!(g->flags & DIFFERS_FROM_STABLE)) {
                                        in_interaction = 0;
                                        printf("log: g%d: interaction %d ends\n", 
                                               g->gen, interaction_nr);
                                }
                        }
                }

                // find the first active generation
                for(g = u_evolving->first; g && !(g->flags & DIFFERS_FROM_STABLE); g = g->next)
                        ;
                generation *g_last;

                if(!g) {
                        klass = 0;
                        goto done;
                }
                ac_first = g->gen;
                if(verbose > 0)
                        printf("First active generation: %d\n", ac_first);

                // find the generation after the last active generation
                g_last = g;
                for(; g; g = g->next) {
                        if(g->flags & DIFFERS_FROM_STABLE)
                                g_last = g;
                }

                if(!g_last) {
                        klass = 1;
                        goto done;
                }

                g = g_last->next ? g_last->next : g_last;
                ac_last = g->gen;
                if(verbose > 0)
                        printf("Last active generation: %d\n", ac_last);

                klass = (2 * ac_first) + (3 * ac_last);

                // The catalyst has returned to its stable state. Any
                // remaining differences are the generated spark.

                // We calculate a hash for each tile independently,
                // and sum them; this way the result is independent of
                // the order in which we traverse the tiles.

                for(t = g->all_first; t; t = t->all_next) {
                        uint32_t hash = 1;
                        tp = universe_find_tile(u_static, 0, t->xpos, t->ypos, 1);
                        for(y=0; y<TILE_HEIGHT; y++) for(x=0; x<TILE_WIDTH; x++) {
                                cellvalue t1 = tile_get_cell(t, x, y);
                                cellvalue t2 = tile_get_cell(tp, x, y);
                                if(t1 != t2) {
                                        hash = (hash ^ t1) * 0xabcdef13;
                                        hash = (hash ^ t2) * 0xabcdef13;
                                        hash = (hash ^ x) * 0xabcdef13;
                                        hash = (hash ^ y) * 0xabcdef13;
                                }
                        }
                        klass += hash;
                }
        done:
                printf("hash: %08x\n", klass);
                break;
        }
        return 0;
}

