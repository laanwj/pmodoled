// Copyright (c) 2017 Wladimir J. van der Laan
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef H_DISPLAY

#include <stdint.h>

/** Display width in pixels */
#define DISP_W 128
/** Display height in pixels */
#define DISP_H 32

/** Initialize pmodoled module */
void pmodoled_init();
/** Initialize SPI */
void spi_init(void);
/** write a byte to OLED spi */
void spi(uint8_t data);
/** wait for completion of queued spi bytes */
void spi_complete(void);
/** set mode to data */
void mode_data(void);
/** set mode to commands */
void mode_cmd(void);

#endif
