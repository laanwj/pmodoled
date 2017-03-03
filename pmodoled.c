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

#include "font.h"

/**
 * Define the following to fall back to GPIO bitbanging,
 * otherwise this demo uses the SPI controller. Use this only
 * for debugging.
 */
//#define SPI_BITBANG

/** SPI clock divider: determines the speed of SPI
 * transfers. This cannot exceed 10Mhz for the SSD1306.
 * CPUfreq is set to 16Mhz in this demo.
 * The formula is CPU_FREQ/(1+SPI_SCKDIV)
 */
#define SPI_SCKDIV (1) /* 16Mhz/(1+1)=8Mhz of max 10Mhz */

/********* Wiring **********
 *
 * GPIO Pin PPin Oled
 * 23   7   1    CS    chip select if low
 * 3    11  2    SDIN
 * 5    13  4    SCLK
 * 2    10  7    D/C   command if low, data if high
 * 0    8   8    RES   reset if low
 * 1    9   9    Vbatc power control display; active-low
 * 4    12  10   Vddc  power control logic; active-low
 * -    Gnd 6/11 GND   ground
 * -    3v3 7/12 VCC   power supply
 *
 * GPIO is the SoC GPIO number
 * Pin is the pin on the HiFive board.
 * PPin is the pin on the PMOD connector. These are laid out as
 *
 *  \/
 * +-------------------+
 * | 1  2  3  4  5  6  |
 * | 7  8  9  10 11 12 |
 * +-------------------+
 *
 * Oled is the name of the pin at the side of the PMODoled module
 *
 ** Hard constraints on wiring:
 *
 * To use SPI controller instead of bitbanging,
 *   Pin 10 should be connected to 7/DC
 *   Pin 11 should be connected to 2/SDIN
 *   Pin 13 should be connected to 4/SDLK
 *
 ** SPI Setup
 *
 * SPI_REG_SCKDIV
 *   Fsck = Fin/(2*(div+1))   Fin=tlclk=cpuclk=16 MHz
 *   So at least 1, which puts the clock rate at 8 MHz (of max 10 MHz)
 *
 * SPI_REG_CSMODE
 *   mode
 *     3 OFF Disable hardware control of the CS pin
 * SPI_REG_SCKMODE
 *   pol
 *     0 Inactive state of SCLK is logical 0
 *   pha
 *     0 Value of SDIN is sampled at SCLK's rising edge so shift out a new bit at falling edge
 * SPI_REG_FMT
 *   proto
 *     0 single
 *   endian
 *     0 Data is clocked from bit 7 (MSB) to bit 0 (LSB)
 *   dir
 *     1 Tx: the receive fifo is not populated
 *   len
 *     8 bits per frame
 */
#define OLED_CS    23
#define OLED_SDIN  3
#define OLED_SCLK  5
#define OLED_DC    2
#define OLED_RES   0
#define OLED_VBATC 1
#define OLED_VDDC  4

/** Display width in pixels */
#define DISP_W 128
/** Display height in pixels */
#define DISP_H 32

/** Startup messages */
static const char led_msg[] = "\a\n\r\n\r\
555555555555555555555555555555555555555555555555\n\r\
5555555 Type text to display on screen 555555555\n\r\
555555555555555555555555555555555555555555555555\n\r";

static const char sifive_msg[] = "\n\r\
\n\r\
               'pmodoled' Demo \n\r\
\n\r";

#ifdef SPI_BITBANG
static void spi_init(void)
{
}

/* write a byte to OLED spi */
static void spi(uint8_t data)
{
    unsigned bit;
    // Value of SDIN is sampled at SCLK's rising edge
    // so put in new bit at falling edge
    // Data is clocked from bit 7 (MSB) to bit 0 (LSB)
    for (bit=0; bit<8; ++bit) {
        GPIO_REG(GPIO_OUTPUT_VAL) &= ~BIT(OLED_SCLK);
        if (data & (1<<(7-bit))) {
            GPIO_REG(GPIO_OUTPUT_VAL) |= BIT(OLED_SDIN);
        } else {
            GPIO_REG(GPIO_OUTPUT_VAL) &= ~BIT(OLED_SDIN);
        }
        GPIO_REG(GPIO_OUTPUT_VAL) |= BIT(OLED_SCLK);
        // Maximum SCLK is 10 MHz
        // Would need to insert delay here at high clock speeds
    }
}
/* wait for completion of queued spi bytes */
static void spi_complete()
{
    /* no-op with synchronous bit-banging */
}
/* set mode */
static void mode_data(void)
{
    GPIO_REG(GPIO_OUTPUT_VAL)  |= BIT(OLED_DC);
}
static void mode_cmd(void)
{
    GPIO_REG(GPIO_OUTPUT_VAL)  &= ~BIT(OLED_DC);
}
#else
static void spi_init(void)
{
    // Select IOF SPI1.MOSI [SDIN] and SPI1.SCK [SLCK] and SPI1.SS0 [DC]
    GPIO_REG(GPIO_IOF_SEL)    &= ~(BIT(OLED_SDIN) | BIT(OLED_SCLK) | BIT(OLED_DC));
    GPIO_REG(GPIO_IOF_EN )    |=  (BIT(OLED_SDIN) | BIT(OLED_SCLK) | BIT(OLED_DC));
    GPIO_REG(GPIO_OUTPUT_VAL) |=  (BIT(OLED_SDIN) | BIT(OLED_SCLK) | BIT(OLED_DC)); /* is this necessary? */

    // Set up SPI controller
    SPI1_REG(SPI_REG_SCKDIV)    = SPI_SCKDIV;
    SPI1_REG(SPI_REG_SCKMODE)   = 0; /* pol and pha both 0 - SCLK is active-high, */
    SPI1_REG(SPI_REG_CSID)      = 0; /* CS 0 */
    SPI1_REG(SPI_REG_CSDEF)     = 0xffff; /* CS is active-low */
    SPI1_REG(SPI_REG_CSMODE)    = SPI_CSMODE_HOLD; /* hold CS where possible */
    /* SPI1_REG(SPI_REG_DCSSCK)    = */
    /* SPI1_REG(SPI_REG_DSCKCS)    = */
    /* SPI1_REG(SPI_REG_DINTERCS)  = */
    /* SPI1_REG(SPI_REG_DINTERXFR) = */
    SPI1_REG(SPI_REG_FMT)       = SPI_FMT_PROTO(SPI_PROTO_S) | SPI_FMT_ENDIAN(SPI_ENDIAN_MSB) | SPI_FMT_DIR(SPI_DIR_TX) | SPI_FMT_LEN(8);
    SPI1_REG(SPI_REG_TXCTRL)    = 1; /* interrupt when <1 in tx fifo (completion) */
    /* SPI1_REG(SPI_REG_RXCTRL)    = */
    /* SPI1_REG(SPI_REG_IE)        = */
}

/* write a byte to OLED spi */
static void spi(uint8_t data)
{
    while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL)
        IDLE;
    SPI1_REG(SPI_REG_TXFIFO) = data;
}
/* wait for completion of queued spi bytes */
static void spi_complete()
{
    // Wait for interrupt condition.
    // It would be more efficient to use an actual interrupt here.
    while (!(SPI1_REG(SPI_REG_IP) & SPI_IP_TXWM))
        IDLE;
}
/* set mode */
static void mode_data(void)
{
    spi_complete(); /* wait for SPI to complete before toggling */
    SPI1_REG(SPI_REG_CSID)      = 1; /* CS 1 */
}
static void mode_cmd(void)
{
    spi_complete(); /* wait for SPI to complete before toggling */
    SPI1_REG(SPI_REG_CSID)      = 0; /* CS 0 */
}
#endif

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

/** Initialize pmodoled module */
void pmodoled_init()
{
    // Set up OLED pins: all are output
    GPIO_REG(GPIO_INPUT_EN)    &= ~(BIT(OLED_CS)|BIT(OLED_SDIN)|BIT(OLED_SCLK)|BIT(OLED_DC)|BIT(OLED_RES)|BIT(OLED_VBATC)|BIT(OLED_VDDC));
    GPIO_REG(GPIO_OUTPUT_EN)   |=  BIT(OLED_CS)|BIT(OLED_SDIN)|BIT(OLED_SCLK)|BIT(OLED_DC)|BIT(OLED_RES)|BIT(OLED_VBATC)|BIT(OLED_VDDC);
#ifndef SPI_BITBANG
    puts("SPI mode: controller\r\n");
#else
    puts("SPI mode: bitbang\r\n");
#endif
    spi_init();

    // Initial setup
    //
    // Select chip
    GPIO_REG(GPIO_OUTPUT_VAL)  &= ~BIT(OLED_CS);
    // Command mode
    GPIO_REG(GPIO_OUTPUT_VAL)  &= ~BIT(OLED_DC);
    // Not reset mode
    GPIO_REG(GPIO_OUTPUT_VAL)  |=  BIT(OLED_RES);
    // Disable power
    GPIO_REG(GPIO_OUTPUT_VAL)  |=  BIT(OLED_VBATC);
    GPIO_REG(GPIO_OUTPUT_VAL)  |=  BIT(OLED_VDDC);

    // Startup sequence
    //
    // 1. Apply power to VDD.
    GPIO_REG(GPIO_OUTPUT_VAL)  &= ~BIT(OLED_VDDC);
    // 2. Send Display Off command (0xAE)
    spi(0xAE);
    spi_complete();
    // Reset
    GPIO_REG(GPIO_OUTPUT_VAL)  &= ~BIT(OLED_RES);
    sleep_ticks(1); // at least 3us
    GPIO_REG(GPIO_OUTPUT_VAL)  |=  BIT(OLED_RES);
    // 3. Initialize display to desired operating mode.
    spi(0x8D); spi(0x14); // charge pump
    spi(0xD9); spi(0xF1); // precharge
    // 4. Clear screen (entire memory)
    spi(0x20); spi(0x00); // horizontal addressing mode
    spi(0x22); spi(0x00); spi(0x07); // page start and end address
    mode_data();
    for (unsigned x=0; x<1024; ++x) {
        spi(0);
    }
    mode_cmd();
    // 5. Apply power to VBAT.
    GPIO_REG(GPIO_OUTPUT_VAL)  &= ~BIT(OLED_VBATC);
    spi(0x81); spi(0x0F); // contrast
    spi(0xA1); spi(0xC8); // invert display
    spi(0xDA); spi(0x20); // comconfig

    // 6. Delay 100ms.
    sleep_ticks(3277);
    // 7. Send Display On command (0xAF).
    spi(0xAF);

    // Display setup
    // spi(0xA5); // full display (only for testing)
    spi(0xA4); // display according to memory
    spi(0x20); spi(0x00); // horizontal addressing mode
    //spi([0x20,0x01]) # vertical addressing mode
    spi(0x22); spi(0x00); spi(0x03); // page start and end address (create wraparound at line 32)
}



int main(void)
{
    // Run off 16 MHz Crystal for accuracy.
    PRCI_REG(PRCI_PLLCFG) = (PLL_REFSEL(1) | PLL_BYPASS(1));
    PRCI_REG(PRCI_PLLCFG) |= (PLL_SEL(1));
    // Turn off HFROSC to save power
    PRCI_REG(PRCI_HFROSCCFG) = 0;

    uart_init();

    puts(sifive_msg);
    puts(led_msg);

    rgb_init();

    pmodoled_init();

    // Data mode is assumed the default throughout the program
    mode_data();
#if 0
    outch('t');
    outch('e');
    outch('s');
    outch('t');

    uint16_t r=0xFF;
    uint16_t g=0;
    uint16_t b=0;
    unsigned led_bright = 3;
    char c = 0;
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
            } else {
                outch(c);
                _putc(c);
            }
        }
    }
#elif 0
    int frame = 0;
    while (1) {
#if 0 /* Function */
        sleep_ticks(300);
        for (int row=0; row<4; ++row) {
            for (int x=0; x<DISP_W; ++x) {
                uint8_t byte = 0;
                for (int yi=0; yi<8; ++yi) {
                    int y = row*8+yi;
                    int val = x * y - frame;
                    //int bit = val >= -1 && val <= 1;
                    int bit = (val & 255)==0;
                    byte |= (bit << yi);
                }
                spi(byte);
            }
        }
#elif 1  /* Perspective effect */
#define N (100)
        uint8_t rows[32] = {0};
        int z = - frame + 50;
        for (int i=0; i<16; ++i) {
            int zi = z + i*N;
            if (zi <= 0) { /* behind camera */
                continue;
            }
            int y = (65536/zi)>>3;
            if (y >= 0 && y < 32) {
                rows[y] = 1;
            }
        }
        for (int row=0; row<4; ++row) {
            for (int x=0; x<DISP_W; ++x) {
                uint8_t byte = 0;
                for (int yi=0; yi<8; ++yi) {
                    byte |= (rows[row*8+yi] << yi);
                }
                spi(byte);
            }
        }
        if (frame > N)
            frame = 0;
#endif
        frame += 1;
    }
#elif 0 /* Mandelbrot (floating point) */
    int frame = 0;
    float basex = -2.0f;
    float basey = -1.0f;
    float stepx = 3.0f / 128.0f;
    float stepy = 2.0f / 32.0f;

    while (1) {
        int itmax = frame;

        for (int row=0; row<4; ++row) {
            for (int x=0; x<DISP_W; ++x) {
                uint8_t byte = 0;
                for (int yi=0; yi<8; ++yi) {
                    int y = row*8+yi;
                    float cx = basex + x * stepx;
                    float cy = basey + y * stepy;
                    /* Z = 0 */
                    float zx = 0.0f;
                    float zy = 0.0f;
                    int it;
                    for (it=0; it<itmax; ++it) {
                        float zx2 = zx*zx;
                        float zy2 = zy*zy;
                        /* |Z| <= 2 */
                        if (zx2 + zy2 > 4.0f) {
                            break;
                        }
                        /* Z = Z^2 + C */
                        float twozxy = 2.0*zx*zy;
                        zx = zx2 - zy2 + cx;
                        zy = twozxy + cy;
                    }

                    int bit = it < itmax;
                    byte |= (bit << yi);
                }
                spi(byte);
            }
        }
        frame += 1;
    }
#elif 1 /* Mandelbrot (fixed point) */
#define PREC 32  /* number of precision bits */
#define I(x) (((int64_t)(x))<<PREC) /* integral value */
/* stupid multiplication of two fp_t */
#define MUL(x,y)  (((x)>>(PREC/2)) * ((y)>>(PREC/2)))
typedef int64_t fp_t;
    int frame = 0;
#if 0
    fp_t centerx = I(-1)/2;
    fp_t centery = I(0);
#elif 1
    fp_t centerx = I(-1)/2;
    fp_t centery = I(3)/4;
#endif
    while (1) {
        fp_t radiusx = I(3)/2 - frame*150000000LL;
        fp_t radiusy = I(2)/2 - frame*150000000LL;
        fp_t basex = centerx - radiusx;
        fp_t basey = centery - radiusy;
        fp_t stepx = 2 * radiusx / DISP_W;
        fp_t stepy = 2 * radiusy / DISP_H;
        int itmax = 10;

        if (radiusx < 0 || radiusy < 0) {
            frame = 0;
            centerx = ((fp_t)mrand48() << 2) + I(-1)/2;
            centery = ((fp_t)mrand48() << 1);
            continue;
        }

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
                    for (it=0; it<itmax; ++it) {
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
            }
        }
        frame += 1;
    }
#endif
}
