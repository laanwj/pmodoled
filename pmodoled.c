// Copyright (c) 2017 Wladimir J. van der Laan
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
/**
 * Drive Digilent pmodOLED module from a HiFive board
 * - SPI interface attached via GPIO pins
 * - SSD1306 controller
 * - 128x32 display
 * See below under "Wiring" how to wire connect the PMOD to the GPIO.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "platform.h"

#include "uartio.h"
#include "bits.h"
#include "sleep.h"
#include "rgb.h"
#include "display.h"

#include "font.h"

/** Startup messages */
static const char startup_msg[] = "\a\n\r\n\r\
555555555555555555555555555555555555555555555555\n\r\
5555555 Type text to display on screen 555555555\n\r\
555555555555555555555555555555555555555555555555\n\r\
\n\r\
               'pmodoled' Demo \n\r\
\n\r";

const char display_msg[]="[esc exits]";

static void set_x(unsigned col)
{
    mode_cmd();
    spi(0x00 | (col & 0xf));
    spi(0x10 | ((col >> 4)&0xf));
    mode_data();
}
static void set_y(unsigned row)
{
    mode_cmd();
    spi(0xb0 | (row & 0x7));
    mode_data();
}

/** Simple text display */
#define CHAR_W (FONT_W+1)
unsigned col = 0;
unsigned row = 0;

/** Go to new line */
static void newline()
{
    /* Pad current line to end, if necessary */
    for (; col < DISP_W; ++col) {
        spi(0);
    }
    col = 0;
    /* Go to next row */
    row = (row + 1)%4;
    /* Clear it */
    for (unsigned x=0; x<DISP_W; ++x) {
        spi(0);
    }
    /* Go back to row */
    set_y(row);
}

/* write a character to OLED screen.
 * must be in data mode.
 */
static void outch(uint8_t ch)
{
    unsigned x;
    if (col > (DISP_W - CHAR_W)) { /* At end of line */
      newline();
    }
    if (ch >= FONT_N) {
        ch = 0;
    }
    for (x=0; x<FONT_W; ++x) {
        spi(font[ch][x]);
    }
    for (; x<CHAR_W; ++x) { /* Pad with empty columns if necessary */
        spi(0);
    }
    col += CHAR_W;
}

void texttest()
{
    unsigned i;
    col = row = 0;
    for (i=0; display_msg[i]; ++i)
        outch(display_msg[i]);
    newline();

    char c = 0;
    uint16_t r=0xFF;
    uint16_t g=0;
    uint16_t b=0;
    unsigned led_bright = 3;
    while(1) {
        sleep_ticks(300);

        if(r > 0 && b == 0){
            r--;
            g++;
        }
        if(g > 0 && r == 0){
            g--;
            b++;
        }
        if(b > 0 && g == 0){
            r++;
            b--;
        }
        rgb_set(r << led_bright, g << led_bright, b << led_bright);

        // Check for user input
        if (_getc(&c) != 0) {
            if (c == 127) { // backspace
                if (col >= CHAR_W) {
                    col -= CHAR_W;
                    set_x(col); // Reset x pos
                    outch(' '); // Empty current character
                    col -= CHAR_W;
                    set_x(col); // Reset x pos
                }
                _putc('^');
            } else if (c== '\r') { // Newline
                newline();
                _putc('\r');
                _putc('\n');
            } else if (c==27) { // Quit
                break;
            } else {
                outch(c);
                _putc(c);
            }
        }
    }
}

/* Mandelbrot */
#define PREC 48  /* number of precision bits */
#define ITMAX 16 /* number of iterations */
#define I(x) (((int64_t)(x))<<PREC) /* integral value */
#define FRAC(x,y) (I(x)/(y)) /* fraction */
/* stupid multiplication of two fp_t */
#define MUL(x,y)  (((x)>>(PREC/2)) * ((y)>>(PREC/2)))
#define ZOOM_MUL (256L)
typedef int64_t fp_t;

/* Is a point on the mandelbrot set interesting to zoom in on? */
int interesting(fp_t x, fp_t y)
{
    return 1; /* TODO */
}

void mandelbrot()
{
    char c;
    int frame = 0;
    fp_t centerx = I(-1)/2;
    fp_t centery = I(3)/4;
    fp_t start_radiusx = I(3);
    fp_t start_radiusy = I(1);
    fp_t radiusx = start_radiusx;
    fp_t radiusy = start_radiusy;
    while (!_getc(&c)) {
        if (frame == 0) {
            do {
                centerx = ((fp_t)mrand48()<<(PREC-31)) + I(-1)/2;
                centery = ((fp_t)mrand48()<<(PREC-32));
            } while (!interesting(centerx, centery));
            radiusx = start_radiusx;
            radiusy = start_radiusy;
        }
        fp_t basex = centerx - radiusx;
        fp_t basey = centery - radiusy;
        fp_t stepx = 2 * radiusx / DISP_W;
        fp_t stepy = 2 * radiusy / DISP_H;

        if (radiusx < (I(1)>>4) || radiusy < (I(1)>>4)) {
            frame = 0;
            continue;
        }
        uint8_t none = 0xff;
        uint8_t all = 0x00;
        for (int row=0; row<4; ++row) {
            for (int x=0; x<DISP_W; ++x) {
                uint8_t byte = 0;
                for (int yi=0; yi<8; ++yi) {
                    int y = row*8+yi;
                    fp_t cx = basex + x * stepx;
                    fp_t cy = basey + y * stepy;
                    /* Z = 0 */
                    fp_t zx = I(0);
                    fp_t zy = I(0);
                    int it;
                    for (it=0; it<ITMAX; ++it) {
                        fp_t zx2 = MUL(zx,zx);
                        fp_t zy2 = MUL(zy,zy);
                        /* |Z| <= 2 */
                        if (zx2 + zy2 > I(4)) {
                            break;
                        }
                        /* Z = Z^2 + C */
                        fp_t twozxy = 2 * MUL(zx,zy);
                        zx = zx2 - zy2 + cx;
                        zy = twozxy + cy;
                    }

                    //int bit = it < itmax;
                    int bit = it&1;
                    byte |= (bit << yi);
                }
                spi(byte);
                all |= byte;
                none &= byte;
            }
        }
        if (all == 0x00 || none == 0xff) {
            /* If screen empty or full, restart */
            frame = 0;
            continue;
        }
        frame += 1;
        radiusx = (radiusx * (ZOOM_MUL-1))/ZOOM_MUL;
        radiusy = (radiusy * (ZOOM_MUL-1))/ZOOM_MUL;
    }
}

int main(void)
{
    puts(startup_msg);

    rgb_init();

    pmodoled_init();

    // Data mode is assumed the default throughout the program
    mode_data();

    while (1) {
        // Mode: mandelbrot
        pmodoled_clear();
        mandelbrot();

        // Mode: text test
        pmodoled_clear();
        texttest();
    }
}
