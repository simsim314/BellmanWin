#ifndef UNIVERSE_DOT_H
#define UNIVERSE_DOT_H

#include <stdint.h>

#if 0
#define TILE_WORD uint32_t
#define TILE_WIDTH 32
#else
#define TILE_WORD uint64_t
#define TILE_WIDTH 64
#endif

#define TILE_HEIGHT 32

enum {
        OFF            = 0x0,
        ON             = 0x1,
        UNKNOWN        = 0x2,
        UNKNOWN_STABLE = 0x3
};

typedef unsigned char cellvalue;

typedef struct tile_s tile;
typedef struct generation_s generation;
typedef struct universe_s universe;

enum {
        EXPAND_UP = 0x01,
        EXPAND_LEFT = 0x02,
        EXPAND_RIGHT = 0x04,
        EXPAND_DOWN = 0x08,

        ACTIVE = 0x10,
        ABORT = 0x20,
        HAS_UNKNOWN_SUCCESSORS = 0x40,
        HAS_ACTIVE_SUCCESSORS = 0x80,
        
        DIFFERS_FROM_STABLE = 0x100,
        HAS_UNKNOWN_CELLS = 0x200,

        IS_DEAD = 0x400,
        CHANGED = 0x800,

        IN_FORBIDDEN_REGION = 0x1000,
        DIFFERS_FROM_PREVIOUS = 0x2000,
        DIFFERS_FROM_2PREV = 0x4000,
        IS_LIVE = 0x8000,

        FILTER_MISMATCH = 0x10000

};

typedef unsigned int evolve_result;

struct tile_s {
        int xpos, ypos;
        tile *left, *right, *up, *down, *prev, *next;
        tile *hashnext;
        tile *all_next;
        TILE_WORD bit0[TILE_HEIGHT], bit1[TILE_HEIGHT];
        void *auxdata;
        evolve_result flags;
        unsigned int n_active; // number of cells that differ from the stable state
        unsigned int delta_prev; // number of cells that differ from the previous generation
        char *text;
        tile *filter; // used by bellman
};

#define HASH_SIZE 15

struct generation_s {
        universe *u;
        uint32_t gen;
        int ntiles;
        generation *next, *prev;
        tile *xyhash[HASH_SIZE];
        tile *all_first, *all_last;
        evolve_result flags;
        unsigned int n_active; // number of cells that differ from the stable state
        unsigned int delta_prev; // number of cells that differ from the previous generation
};

struct universe_s {
        uint32_t n_gens;
        cellvalue def;
        generation *first, *last;
};

universe *universe_new(cellvalue def);
void universe_free(universe *);

universe *universe_copy(universe *from, int gen);

generation *universe_find_generation(universe *u, uint32_t gen, int create);
tile *generation_find_tile(generation *g, int xpos, int ypos, int create);

tile *universe_find_tile(universe *u, 
                         uint32_t g, uint32_t x, uint32_t y,
                         int create);

cellvalue tile_get_cell(tile *t, uint32_t x, uint32_t y);
void tile_set_cell(tile *t, uint32_t x, uint32_t y, cellvalue v);

void tile_set_flags(tile *t);

void generation_set_cell(generation *g, int x, int y, cellvalue v);

void universe_evolve(universe *u, uint32_t gen);
void universe_evolve_next(universe *u);

typedef evolve_result evolve_func(tile *t, tile *out);

void generation_evolve(generation *g, evolve_func *func);

evolve_func tile_evolve_simple;
evolve_func tile_evolve_bitwise;
evolve_func tile_evolve_bitwise_3state;
evolve_func tile_stabilise_3state;

universe *find_still_life(universe *);

char tile_get_text(tile *t, int x, int y);
void tile_set_text(tile *t, int x, int y, char c);

char generation_get_text(generation *g, int x, int y);
void generation_set_text(generation *g, int x, int y, char c);

void generation_to_text(generation *g);

void generation_find_bounds(generation *, int *l, int *r, int *t, int *b);
void tile_find_bounds(tile *, int *l, int *r, int *t, int *b);
void tile_find_bounds_text(tile *, int *l, int *r, int *t, int *b);

const char *flag2str(evolve_result flags);

#endif
