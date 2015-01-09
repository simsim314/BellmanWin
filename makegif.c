#include "gd.h"

#include "universe.h"
#include "readwrite.h"

#define GX(x) ((x - l) * scale)
#define GY(y) ((y - t) * scale)

void write_gif(universe *u, int gen, const char *filename) {

        FILE *out = fopen(filename, "wb");
        gdImagePtr im;

        int l, r, t, b;

        generation *g = universe_find_generation(u, gen, 0);
        generation_find_bounds(g, &l, &r, &t, &b);

        int scale = 4;

        im = gdImageCreate((r - l) * scale, (b - t) * scale);

        int background = gdImageColorAllocate(im, 230, 230, 255);
        gdImageRectangle(im, 0, 0, (r-l) * scale, (b-t) * scale, background);

        int tile_bg = gdImageColorAllocate(im, 255, 255, 255);
        int tile_frame = gdImageColorAllocate(im, 230, 230, 230);

        int on = gdImageColorAllocate(im, 0, 0, 0);
        int unknown = gdImageColorAllocate(im, 255, 255, 0);
        int unknown_stable = gdImageColorAllocate(im, 192, 255, 0);

        tile *tp;
        for(tp = g->all_first; tp; tp = tp->all_next) {
                if(!(tp->flags & (IS_LIVE | HAS_UNKNOWN_CELLS))) {
                        continue;
                }

                gdImageFilledRectangle(im, GX(tp->xpos), GY(tp->ypos),
                                       GX(tp->xpos + TILE_WIDTH)-1, 
                                       GY(tp->ypos + TILE_HEIGHT)-1,
                                       tile_bg);

                gdImageRectangle(im, GX(tp->xpos), GY(tp->ypos),
                                 GX(tp->xpos + TILE_WIDTH)-1, 
                                 GY(tp->ypos + TILE_HEIGHT)-1,
                                 tile_frame);
                int xx, yy;
                for(xx = 0; xx < TILE_WIDTH; xx++) {
                        for(yy = 0; yy < TILE_HEIGHT; yy++) {
                                cellvalue cv = tile_get_cell(tp, xx, yy);
                                if(cv == ON) {
                                        gdImageFilledRectangle(im, 
                                                               GX(tp->xpos + xx),
                                                               GY(tp->ypos + yy),
                                                               GX(tp->xpos + xx + 1) - 1,
                                                               GY(tp->ypos + yy + 1) - 1,
                                                               on);
                                } else if(cv == UNKNOWN) {
                                        gdImageFilledRectangle(im, 
                                                               GX(tp->xpos + xx),
                                                               GY(tp->ypos + yy),
                                                               GX(tp->xpos + xx + 1) - 1,
                                                               GY(tp->ypos + yy + 1) - 1,
                                                               unknown);
                                } else if(cv == UNKNOWN_STABLE) {
                                        gdImageFilledRectangle(im, 
                                                               GX(tp->xpos + xx),
                                                               GY(tp->ypos + yy),
                                                               GX(tp->xpos + xx + 1) - 1,
                                                               GY(tp->ypos + yy + 1) - 1,
                                                               unknown_stable);
                                } 
                        }
                }
        }

        gdImageGif(im, out);
        fclose(out);
        gdImageDestroy(im);
}
/*
void write_animated_gif(universe *u, const char *filename) {

        FILE *out = fopen(filename, "wb");
        gdImagePtr im;

        int l, r, t, b;
        int ll, rr, tt, bb;

        generation *g = u->first;
        generation_find_bounds(g, &l, &r, &t, &b);

        for(g = g->next; g; g = g->next) {
                generation_find_bounds(g, &ll, &rr, &tt, &bb);
                if(ll < l) l = ll;
                if(rr > r) r = rr;
                if(tt < t) t = tt;
                if(bb > b) b = bb;
        }

        int scale = 4;
	int delay = 10;

        im = gdImageCreate((r - l) * scale, (b - t) * scale);

        int background = gdImageColorAllocate(im, 230, 230, 255);
        gdImageRectangle(im, 0, 0, (r-l) * scale, (b-t) * scale, background);

        int tile_bg = gdImageColorAllocate(im, 255, 255, 255);
        int tile_frame = gdImageColorAllocate(im, 230, 230, 230);

        int on = gdImageColorAllocate(im, 0, 0, 0);
        int unknown = gdImageColorAllocate(im, 255, 255, 0);
        int unknown_stable = gdImageColorAllocate(im, 192, 255, 0);

	gdImageGifAnimBegin(im, out, 1, 0);

        for(g = u->first; g; g = g->next) {
                tile *tp;
                for(tp = g->all_first; tp; tp = tp->all_next) {
                        if(!(tp->flags & IS_LIVE)) {
                                continue;
                        }

                        gdImageFilledRectangle(im, GX(tp->xpos), GY(tp->ypos),
                                               GX(tp->xpos + TILE_WIDTH)-1, 
                                               GY(tp->ypos + TILE_HEIGHT)-1,
                                               tile_bg);

                        gdImageRectangle(im, GX(tp->xpos), GY(tp->ypos),
                                         GX(tp->xpos + TILE_WIDTH)-1, 
                                         GY(tp->ypos + TILE_HEIGHT)-1,
                                         tile_frame);
                        int xx, yy;
                        for(xx = 0; xx < TILE_WIDTH; xx++) {
                                for(yy = 0; yy < TILE_HEIGHT; yy++) {
                                        cellvalue cv = tile_get_cell(tp, xx, yy);
                                        int colour = -1;
                                        if(cv == ON) {
                                                colour = on;
                                        } else if(cv == UNKNOWN) {
                                                colour = unknown;
                                        } else if(cv == UNKNOWN_STABLE) {
                                                colour = unknown_stable;
                                        } 
                                        if(colour >= 0)
                                                gdImageFilledRectangle(im, 
                                                                       GX(tp->xpos + xx),
                                                                       GY(tp->ypos + yy),
                                                                       GX(tp->xpos + xx + 1) - 1,
                                                                       GY(tp->ypos + yy + 1) - 1,
                                                                       colour);
                                }
                        }
                }
		gdImageGifAnimAdd(im, out, 0, 0, 0,
				  delay, gdDisposalNone, NULL);
        }

	gdImageGifAnimEnd(out);

        fclose(out);
        gdImageDestroy(im);
}
*/
