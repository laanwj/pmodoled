#include "display.h"

#include <stdio.h>
#include <stdlib.h>
#include "platform.h"

#include "sleep.h"
#include "bits.h"

/**
 * Define the following to fall back to GPIO bitbanging,
 * otherwise this demo uses the SPI controller. Use this only
 * for debugging.
 */
//#define SPI_BITBANG

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

/** SPI speed cannot exceed 10MHz for SSD1306 */
#define MAX_SPI_FREQ (10000000)

#ifdef SPI_BITBANG
void spi_init(void)
{
}

void spi(uint8_t data)
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
void spi_complete()
{
    /* no-op with synchronous bit-banging */
}
void mode_data(void)
{
    GPIO_REG(GPIO_OUTPUT_VAL)  |= BIT(OLED_DC);
}
void mode_cmd(void)
{
    GPIO_REG(GPIO_OUTPUT_VAL)  &= ~BIT(OLED_DC);
}

#else

void spi_init(void)
{
    // Select IOF SPI1.MOSI [SDIN] and SPI1.SCK [SLCK] and SPI1.SS0 [DC]
    GPIO_REG(GPIO_IOF_SEL)    &= ~(BIT(OLED_SDIN) | BIT(OLED_SCLK) | BIT(OLED_DC));
    GPIO_REG(GPIO_IOF_EN )    |=  (BIT(OLED_SDIN) | BIT(OLED_SCLK) | BIT(OLED_DC));
    GPIO_REG(GPIO_OUTPUT_VAL) |=  (BIT(OLED_SDIN) | BIT(OLED_SCLK) | BIT(OLED_DC)); /* is this necessary? */

    // Set up SPI controller
    /** SPI clock divider: determines the speed of SPI
     * transfers. This cannot exceed 10Mhz for the SSD1306.
     * CPUfreq is set to 16Mhz in this demo.
     * The formula is CPU_FREQ/(1+SPI_SCKDIV)
     */
    SPI1_REG(SPI_REG_SCKDIV)    = (get_cpu_freq() / MAX_SPI_FREQ) - 1;
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

void spi(uint8_t data)
{
    while (SPI1_REG(SPI_REG_TXFIFO) & SPI_TXFIFO_FULL)
        IDLE;
    SPI1_REG(SPI_REG_TXFIFO) = data;
}

void spi_complete()
{
    // Wait for interrupt condition.
    // It would be more efficient to use an actual interrupt here.
    while (!(SPI1_REG(SPI_REG_IP) & SPI_IP_TXWM))
        IDLE;
}

void mode_data(void)
{
    spi_complete(); /* wait for SPI to complete before toggling */
    SPI1_REG(SPI_REG_CSID)      = 1; /* CS 1 */
}

void mode_cmd(void)
{
    spi_complete(); /* wait for SPI to complete before toggling */
    SPI1_REG(SPI_REG_CSID)      = 0; /* CS 0 */
}
#endif

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

void pmodoled_clear(void)
{
    mode_cmd();
    spi(0x22); spi(0x00); spi(0x03); // page start and end address (create wraparound at line 32)
    mode_data();
    for (unsigned x=0; x<512; ++x) {
        spi(0);
    }
}

