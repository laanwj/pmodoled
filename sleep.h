// Copyright (c) 2017 Wladimir J. van der Laan
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef H_SLEEP
#define H_SLEEP
/* Yawn */

#define IDLE asm volatile ("")

/* Sleep n 32768 Hz ticks */
static void sleep_ticks(uint32_t n)
{
    uint64_t now = get_timer_value();
    while (get_timer_value() - now < n)
        IDLE;
}

#endif

