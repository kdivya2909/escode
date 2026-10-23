/*
 * ============================================================================
 *  Topic   : 01 Introduction to C
 *  Example : 03 - The firmware shape: init once, then loop forever (the "super-loop")
 *  Flavour : MCU
 *  Level   : Basic
 * ----------------------------------------------------------------------------
 *  Concept : Bare-metal main() has two phases: (1) set up the hardware once, (2) an endless
 *            while loop that does the work again and again. It must NEVER return - there is
 *            no operating system to return to. Here the loop is bounded (keep_running()) only
 *            so the PC demo can finish; on a board that function would simply return 1.
 *  Embedded use : every blinky, every Arduino sketch (setup() + loop()) and most small
 *            products use this structure.
 *  Real STM32F4 : while (1) { GPIOA->ODR ^= (1U << 5); HAL_Delay(500); }
 *  Build   : gcc -std=gnu11 -Wall -Wextra -I../../common ex03_blink_super_loop.c -o ex03
 *  Output  : two LEDs (PA5, PA6) swap on/off every 500 ms for 8 passes through the loop
 * ----------------------------------------------------------------------------
 *  Try it  : 1) Change BLINK_MS to 250 - how do the timestamps change?
 *            2) Start with both LEDs on (ODR = 0x60). Does the wig-wag pattern still alternate?
 * ============================================================================
 */
#include "blink.h"

#define LED_A    5U             /* PA5 */
#define LED_B    6U             /* PA6 */
#define BLINK_MS 500U

/* Phase 1: runs once at power-up. */
static void system_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;                          /* clock the GPIOA block */
    GPIOA->MODER &= ~((3U << (LED_A * 2U)) | (3U << (LED_B * 2U)));
    GPIOA->MODER |=   (1U << (LED_A * 2U)) | (1U << (LED_B * 2U)); /* both pins = output   */
    GPIOA->ODR = (1U << LED_A);                                   /* start: A on, B off    */
}

/* Real firmware: return 1;  The demo stops after 8 passes so that the program can exit. */
static int keep_running(void)
{
    static unsigned passes_left = 8U;
    return passes_left-- > 0U;
}

int main(void)
{
    system_init();
    puts("init done, entering the super-loop");
    sim_show_leds("PA6 PA5 start", (GPIOA->ODR >> LED_A) & 3U, 2);

    unsigned pass = 0U;
    while (keep_running()) {                                      /* while (1) on a board  */
        GPIOA->ODR ^= (1U << LED_A) | (1U << LED_B);              /* swap both LEDs at once */
        sim_delay_ms(BLINK_MS);                                   /* HAL_Delay(500)         */
        pass++;
        char label[24];
        snprintf(label, sizeof label, "PA6 PA5 pass %u", pass);
        sim_show_leds(label, (GPIOA->ODR >> LED_A) & 3U, 2);
    }

    /* On firmware the line below is never reached; the startup code would spin forever. */
    puts("loop ended (only because this is a PC demo)");
    return 0;
}

