#include <msx.h>
#include <vdp.h>
#include <screen.h>
#include <text.h>
#include <ZX0_decompress.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "gfx/nave.h"
#include "gfx/vram.h"

// Defines
#define CHAR_FRAMES     8
#define BACK_BUFFER     768

#define SCORE_HEIGHT    20
#define CHAR_OFFY       SCORE_HEIGHT+2
#define TILE_OFFY       CHAR_OFFY+16

#define TILEW           16
#define TILEH           16
#define SPLIT_SCORE     (SCORE_HEIGHT-6)

#define MAX_SP_LOG      52
#define MAX_SP_PHYS     32
#define MAX_NOFLICK     12
#define MAX_SHOOTS      MAX_SP_LOG-MAX_NOFLICK

#define SWIDTH          256
#define SHEIGHT         212

const uint8_t map[] =
{
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

volatile uint8_t    decompress[5120];

inline uint8_t hscroll_register_r26_value_from(uint16_t x) {
  return ((uint8_t)((x + 7) >> 3)) & 0x3f;
}

inline uint8_t hscroll_register_r27_value_from(uint16_t x) {
  return (-((uint8_t)x)) & 7;
}

inline void enable_hsync_interrupt(void) {
  VDP_SET_CONTROL_REGISTER(0, (RG0SAV |= 0x10));
}

inline void disable_hsync_interrupt(void) {
  VDP_SET_CONTROL_REGISTER(0, (RG0SAV &= ~0x10));
}

uint8_t *p;
size_t counter;

inline void vdp_copy_HMMC(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t *data,  size_t count){
    vdp_cmd_execute_HMMC(x, y, w, h, VDP_CMD_LRTB);
    p = data;
    counter = count;
    for (; counter--; ) {
        vdp_cmd_write(*p);
        ++p;
    }
}

inline void vdp_set_page(uint16_t page){
    vdp_set_image_table(0x8000ULL*page);
}

// \note same as `vdp_set_image_table(loc)` but optimal.
/*inline void on_vsync(void) {
  // select VRAM page #1
  // \note same as `vdp_set_image_table(0x08000LL)` but optimal.
  VDP_SET_CONTROL_REGISTER(2, RG2SAV = ((0x08000LL >> 10) & 0x7F) | 0x1F);
  // reset scroll registers
  VDP_SET_CONTROL_REGISTER(23, 0);
  VDP_SET_CONTROL_REGISTER(26, 0);
  VDP_SET_CONTROL_REGISTER(27, 0);
  // setup HSYNC interrupt line to HSYNC_LINE0 (top of the landspcape area)
  VDP_SET_CONTROL_REGISTER(19, (RG19SA = (uint8_t)(HSYNC_LINE0 - 6)));
  start_raster = false;
  enable_hsync_interrupt();
}

inline void on_hsync(void) {
  disable_hsync_interrupt();
  if (!start_raster) {
    // select VRAM page #3
    // \note same as `vdp_set_image_table(0x18000LL)` but optimal.
    VDP_SET_CONTROL_REGISTER(2, RG2SAV = ((0x18000LL >> 10) & 0x7F) | 0x1F);
    // scroll the distant landscape area
    VDP_SET_CONTROL_REGISTER(23, bg_scroll_y);
    VDP_SET_CONTROL_REGISTER(27, hscroll_register_r27_value_from(bg_scroll_x));
    VDP_SET_CONTROL_REGISTER(26, hscroll_register_r26_value_from(bg_scroll_x));
    // setup HSYNC interrupt line to HORIZON_LINE (top of the ground area)
    VDP_SET_CONTROL_REGISTER(19, (RG19SA = (uint8_t)(bg_scroll_y + HORIZON_LINE - 6)));
    start_raster = true;
    enable_hsync_interrupt();
    return;
  }
  // select VRAM page #1
  // \note same as `vdp_set_image_table(0x08000LL)` but optimal.
  VDP_SET_CONTROL_REGISTER(2, RG2SAV = ((0x08000LL >> 10) & 0x7F) | 0x1F);
  // scroll the ground area every LINES_PER_ROW lines (raster scroll)
  }
}

void interrupt_handler(void) {
  if (VDP_GET_STATUS_REGISTER_VALUE() & 0x80) {
    on_vsync();
    JIFFY++;
    vsync_busy = false;
  }
  VDP_SET_STATUS_REGISTER_POINTER(1);
  if (VDP_GET_STATUS_REGISTER_VALUE() & 1) {
    on_hsync();
  }
  VDP_SET_STATUS_REGISTER_POINTER(0);
  __asm__("ei");
}*/

void main(void) {
    screen5();
    color(0, 0, 0);
    cls();

    // SET PAGE 0
    vdp_set_page(0);

    // Sets the palette
    vdp_write_palette(vram_pal);
    // Decompress the VRAM data in 5k segments
    ZX0_decompress (vram1, decompress);
    vdp_copy_HMMC(0, CHAR_OFFY,256,32, decompress,  5120);
    ZX0_decompress (vram2, decompress);
    vdp_copy_HMMC(0, CHAR_OFFY+32,256,32, decompress,  5120);
    ZX0_decompress (vram3, decompress);
    vdp_copy_HMMC(0, CHAR_OFFY+64,256,16, decompress,  2560);

    // Draws the first display buffer (page 1)
    vdp_set_page(1);
    for(uint8_t l = 0; l < 16; ++l)
        for(uint8_t c = 0; c < 16; ++c){
            // The +256 in the y coordinate is to draw on page 1
            vdp_cmd_execute_HMMM((map[l*16+c]%16)*16,(map[l*16+c]/16)*16+TILE_OFFY, 16, 16, c*16,l*16+256);
        }


  // setup HSYNC / VSYNC interrupt handler
  //set_interrupt_handler(interrupt_handler);


  for (;;)
    await_vsync();

}
