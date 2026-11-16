/* FNK0104S: retain the game's 448x368 coordinate system, but fill 480x320 glass.
 * Its FT6336U reports native portrait coordinates (320x480). The panel's
 * landscape rotation 1 places native touch (px,py) at glass (py,319-px),
 * as Freenove's Sketch_12.1_TFT_Touch_Draw does. Keep the
 * display sampler and touch mapper tied to the exact same transform. */
#ifndef FREENOVE_GEOMETRY_H
#define FREENOVE_GEOMETRY_H

#include "board_pins.h"
#include "tank.h"

#define FNK_GLASS_W F_PANEL_H
#define FNK_GLASS_H F_PANEL_W

/* Sample pixel centers so the bottom and right edges remain reachable too. */
static inline int fnk_frame_x(int glass_x) { return (2 * glass_x + 1) * TANK_W / (2 * FNK_GLASS_W); }
static inline int fnk_frame_y(int glass_y) { return (2 * glass_y + 1) * TANK_H / (2 * FNK_GLASS_H); }
static inline int fnk_touch_x(int portrait_y) { return fnk_frame_x(portrait_y); }
static inline int fnk_touch_y(int portrait_x) { return fnk_frame_y(FNK_GLASS_H - 1 - portrait_x); }

#endif
