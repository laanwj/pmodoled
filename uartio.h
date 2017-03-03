// From the "led_fade" demo (Apache 2 license).
#ifndef H_UARTIO
#define H_UARTIO
/* Unbuffered UART I/O */

static void _putc(char c)
{
    while ((int32_t) UART0_REG(UART_REG_TXFIFO) < 0);
    UART0_REG(UART_REG_TXFIFO) = c;
}

static int _getc(char * c)
{
    int32_t val = (int32_t) UART0_REG(UART_REG_RXFIFO);
    if (val > 0) {
        *c =  val & 0xFF;
        return 1;
    }
    return 0;
}

#endif
