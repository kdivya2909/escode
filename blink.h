/*
 * ============================================================================
 *  hw_sim.h  -  Simulated STM32F4-style peripherals for running on a PC
 * ============================================================================
 *  Purpose
 *    Lets you teach real register-level embedded C (GPIOA->ODR |= (1U << 5))
 *    without any board. Everything here is plain memory on your PC.
 *
 *  How it maps to real hardware
 *    Real STM32 (CMSIS)          This header
 *    -------------------------   ------------------------------------------
 *    GPIO_TypeDef, GPIOA         same names (GPIOA expands to sim_gpio(0))
 *    RCC->AHB1ENR                same name, plain variable
 *    HAL_Delay(ms)               sim_delay_ms(ms)   (no real waiting)
 *    HAL_GetTick()               sim_millis()
 *
 *  The one difference to remember (a good discussion point with students!)
 *    On real silicon the GPIO hardware reacts to a BSRR write instantly and
 *    a pin driven as output is readable in IDR instantly. On the PC nothing
 *    is "listening", so we apply those side effects when you call
 *    sim_delay_ms(), sim_sync() or any sim_show_*() function.
 *
 *  Compile (from a topic folder):
 *    gcc -std=gnu11 -Wall -Wextra -I../../common prog.c -o prog
 *  Optional: add -DSIM_REALTIME to make sim_delay_ms() really sleep.
 * ============================================================================
 */
#ifndef HW_SIM_H
#define HW_SIM_H

#include <stdint.h>
#include <stdio.h>

#if defined(SIM_REALTIME)
#  if defined(_WIN32)
#    include <windows.h>
#  else
#    include <time.h>
#  endif
#endif

/* ---------------------------------------------------------------------------
 * Register layouts (same member order/offsets as STM32F4 reference manual)
 * ------------------------------------------------------------------------- */
typedef struct {
    volatile uint32_t MODER;    /* 0x00 pin mode: 00 input, 01 output, 10 alt-fn, 11 analog */
    volatile uint32_t OTYPER;   /* 0x04 output type: 0 push-pull, 1 open-drain               */
    volatile uint32_t OSPEEDR;  /* 0x08 output speed (2 bits per pin)                        */
    volatile uint32_t PUPDR;    /* 0x0C pull-up / pull-down (2 bits per pin)                 */
    volatile uint32_t IDR;      /* 0x10 input data (read-only on real HW)                    */
    volatile uint32_t ODR;      /* 0x14 output data                                          */
    volatile uint32_t BSRR;     /* 0x18 bit set (low 16) / bit reset (high 16), write-only   */
    volatile uint32_t LCKR;     /* 0x1C lock register                                        */
    volatile uint32_t AFR[2];   /* 0x20 alternate function low/high                          */
} GPIO_TypeDef;

/* Same layout and offsets as the STM32F4 RCC block (RM0090), so AHB1ENR really sits at 0x30. */
typedef struct {
    volatile uint32_t CR;            /* 0x00 clock control                         */
    volatile uint32_t PLLCFGR;       /* 0x04 PLL configuration                     */
    volatile uint32_t CFGR;          /* 0x08 clock configuration                   */
    volatile uint32_t CIR;           /* 0x0C clock interrupt                       */
    volatile uint32_t AHB1RSTR;      /* 0x10 AHB1 peripheral reset                 */
    volatile uint32_t AHB2RSTR;      /* 0x14 AHB2 peripheral reset                 */
    volatile uint32_t AHB3RSTR;      /* 0x18 AHB3 peripheral reset                 */
    uint32_t          RESERVED0;     /* 0x1C                                       */
    volatile uint32_t APB1RSTR;      /* 0x20 APB1 peripheral reset                 */
    volatile uint32_t APB2RSTR;      /* 0x24 APB2 peripheral reset                 */
    uint32_t          RESERVED1[2];  /* 0x28 - 0x2C                                */
    volatile uint32_t AHB1ENR;       /* 0x30 AHB1 peripheral clock enable (GPIOs!) */
    volatile uint32_t AHB2ENR;       /* 0x34 AHB2 peripheral clock enable          */
    volatile uint32_t AHB3ENR;       /* 0x38 AHB3 peripheral clock enable          */
    uint32_t          RESERVED2;     /* 0x3C                                       */
    volatile uint32_t APB1ENR;       /* 0x40 APB1 peripheral clock enable          */
    volatile uint32_t APB2ENR;       /* 0x44 APB2 peripheral clock enable          */
} RCC_TypeDef;

#define SIM_NUM_PORTS 4u        /* GPIOA .. GPIOD */

/* Function-local statics keep this header "header-only" without warnings. */
static inline GPIO_TypeDef *sim_gpio(unsigned idx)
{
    static GPIO_TypeDef ports[SIM_NUM_PORTS];
    return &ports[idx < SIM_NUM_PORTS ? idx : 0u];
}

static inline RCC_TypeDef *sim_rcc(void)
{
    static RCC_TypeDef rcc;
    return &rcc;
}

#define GPIOA (sim_gpio(0))
#define GPIOB (sim_gpio(1))
#define GPIOC (sim_gpio(2))
#define GPIOD (sim_gpio(3))
#define RCC   (sim_rcc())

/* Clock-enable bits in RCC->AHB1ENR (same positions as STM32F4) */
#define RCC_AHB1ENR_GPIOAEN (1U << 0)
#define RCC_AHB1ENR_GPIOBEN (1U << 1)
#define RCC_AHB1ENR_GPIOCEN (1U << 2)
#define RCC_AHB1ENR_GPIODEN (1U << 3)

/* ---------------------------------------------------------------------------
 * Simulated hardware behaviour
 * ------------------------------------------------------------------------- */

/* Apply what the GPIO hardware would do "instantly" on a real chip. */
static inline void sim_gpio_apply(GPIO_TypeDef *p)
{
    uint32_t b = p->BSRR;
    if (b != 0u) {
        uint32_t set = b & 0xFFFFu;
        uint32_t rst = (b >> 16) & 0xFFFFu;
        /* If one write sets AND resets the same pin, SET wins - RM0090: "BSx has priority". */
        p->ODR  = (p->ODR & ~rst) | set;
        p->BSRR = 0u;                      /* BSRR always reads back 0 */
    }
    for (unsigned pin = 0; pin < 16u; pin++) {
        if (((p->MODER >> (pin * 2u)) & 3u) == 1u) {      /* pin is an output */
            if (p->ODR & (1u << pin)) p->IDR |=  (1u << pin);
            else                      p->IDR &= ~(1u << pin);
        }
    }
}

static inline void sim_sync(void)
{
    for (unsigned i = 0; i < SIM_NUM_PORTS; i++) sim_gpio_apply(sim_gpio(i));
}

/* ---- simulated system tick (HAL_GetTick / HAL_Delay) -------------------- */
static inline uint32_t *sim_tick_ref(void)
{
    static uint32_t ticks;
    return &ticks;
}

static inline uint32_t sim_millis(void) { return *sim_tick_ref(); }

static inline void sim_delay_ms(uint32_t ms)
{
    *sim_tick_ref() += ms;
    sim_sync();
#if defined(SIM_REALTIME)
#  if defined(_WIN32)
    Sleep(ms);
#  else
    struct timespec ts = { (time_t)(ms / 1000u), (long)(ms % 1000u) * 1000000L };
    nanosleep(&ts, NULL);
#  endif
#endif
}

/* ---- printing helpers ---------------------------------------------------- */

/* Print the lowest `nbits` of v, MSB first, in groups of four: 0010 0000 */
static inline void sim_print_bits(uint32_t v, unsigned nbits)
{
    for (unsigned i = nbits; i > 0u; i--) {
        putchar((v >> (i - 1u)) & 1u ? '1' : '0');
        if ((i - 1u) % 4u == 0u && i > 1u) putchar(' ');
    }
}

/* e.g.  [   500 ms] GPIOA.ODR = 0x0020  0000 0000 0010 0000 */
static inline void sim_show_port(const char *name, GPIO_TypeDef *p)
{
    sim_gpio_apply(p);
    printf("[%6u ms] %s.ODR = 0x%04X  ", (unsigned)sim_millis(), name, (unsigned)(p->ODR & 0xFFFFu));
    sim_print_bits(p->ODR, 16u);
    putchar('\n');
}

/* LED bar, MSB (left) to LSB (right):  PORTB LEDs  [#][.][.][#]  */
static inline void sim_show_leds(const char *label, uint32_t value, unsigned count)
{
    printf("[%6u ms] %s ", (unsigned)sim_millis(), label);
    for (unsigned i = count; i > 0u; i--)
        printf("[%c]", ((value >> (i - 1u)) & 1u) ? '#' : '.');
    putchar('\n');
}

/* ---- simulated push buttons (active-high input pins) -------------------- */
static inline void sim_press(GPIO_TypeDef *p, unsigned pin)   { p->IDR |=  (1u << pin); }
static inline void sim_release(GPIO_TypeDef *p, unsigned pin) { p->IDR &= ~(1u << pin); }

#endif /* HW_SIM_H */
