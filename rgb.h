// From the "led_fade" demo (Apache 2 license).
#ifndef H_RGB
#define H_RGB

static void rgb_init()
{
    // Set up RGB PWM
    PWM1_REG(PWM_CFG)   = 0;
    // To balance the power consumption, make one left, one right, and one center aligned.
    PWM1_REG(PWM_CFG)   = (PWM_CFG_ENALWAYS) | (PWM_CFG_CMP2CENTER);
    PWM1_REG(PWM_COUNT) = 0;

    // Period is approximately 244 Hz
    // the LEDs are intentionally left somewhat dim, 
    // as the full brightness can be painful to look at.
    PWM1_REG(PWM_CMP0)  = 0;

    GPIO_REG(GPIO_IOF_SEL)    |= ( (1 << GREEN_LED_OFFSET) | (1 << BLUE_LED_OFFSET) | (1 << RED_LED_OFFSET));
    GPIO_REG(GPIO_IOF_EN )    |= ( (1 << GREEN_LED_OFFSET) | (1 << BLUE_LED_OFFSET) | (1 << RED_LED_OFFSET));
    GPIO_REG(GPIO_OUTPUT_XOR) &= ~( (1 << GREEN_LED_OFFSET) | (1 << BLUE_LED_OFFSET));
    GPIO_REG(GPIO_OUTPUT_XOR) |= (1 << RED_LED_OFFSET);
}

static void rgb_set(uint32_t r, uint32_t g, uint32_t b)
{
    PWM1_REG(PWM_CMP1)  = g;          // PWM is low on the left, GPIO is low on the left side, LED is ON on the left.
    PWM1_REG(PWM_CMP2)  = (b << 1);   // PWM is high on the middle, GPIO is low in the middle, LED is ON in the middle.
    PWM1_REG(PWM_CMP3)  = 0xFFFF - r; // PWM is low on the left, GPIO is low on the right, LED is on on the right.
}

#endif
