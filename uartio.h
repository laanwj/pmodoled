// From the "led_fade" demo (Apache 2 license).
#ifndef H_UARTIO
#define H_UARTIO
/* Unbuffered UART I/O */

static void uart_init()
{
    // Configure UART GPIO pins
    GPIO_REG(GPIO_OUTPUT_VAL) |= IOF0_UART0_MASK;
    GPIO_REG(GPIO_OUTPUT_EN)  |= IOF0_UART0_MASK;
    GPIO_REG(GPIO_IOF_SEL)    &= ~IOF0_UART0_MASK;
    GPIO_REG(GPIO_IOF_EN)     |= IOF0_UART0_MASK;
    // RX and TX enable
    UART0_REG(UART_REG_TXCTRL) = UART_TXEN;
    UART0_REG(UART_REG_RXCTRL) = UART_RXEN;
}

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
