#include "universe.h"

static inline TILE_WORD get_word0_left(tile *t, int y) {
        TILE_WORD rv = t->bit0[y] << 1;
        if(t->left) rv |= t->left->bit0[y] >> (TILE_WIDTH-1);
        return rv;
}

static inline TILE_WORD get_word0_right(tile *t, int y) {
        TILE_WORD rv = t->bit0[y] >> 1;
        if(t->right) rv |= t->right->bit0[y] << (TILE_WIDTH-1);
        return rv;
}

static inline TILE_WORD get_word1_left(tile *t, int y) {
        TILE_WORD rv = t->bit1[y] << 1;
        if(t->left) rv |= t->left->bit1[y] >> (TILE_WIDTH-1);
        return rv;
}

static inline TILE_WORD get_word1_right(tile *t, int y) {
        TILE_WORD rv = t->bit1[y] >> 1;
        if(t->right) rv |= t->right->bit1[y] << (TILE_WIDTH-1);
        return rv;
}

#define GET3WORDS(left_r, mid_r, right_r, tile, which, y)       \
        mid_r = tile->bit ## which [y];                       \
        left_r = get_word ## which ## _left(tile, y);          \
        right_r = get_word ## which ## _right(tile, y);       

#define half_adder(out0, out1, in0, in1) \
        TILE_WORD out0, out1;            \
        out0 = in0 ^ in1;                \
        out1 = in0 & in1;

#define full_adder(t, out0, out1, in0, in1, in2)                        \
        TILE_WORD out0, out1;                                           \
        TILE_WORD t ## halftotal, t ## halfcarry1, t ## halfcarry2;     \
        t ## halftotal = in0 ^ in1;                                     \
        out0 = t ## halftotal ^ in2;                                    \
        t ## halfcarry1 = in0 & in1;                                    \
        t ## halfcarry2 = in2 & t ## halftotal;                         \
        out1 = t ## halfcarry1 | t ## halfcarry2;

