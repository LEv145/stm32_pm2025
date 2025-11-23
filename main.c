#include <stdint.h>
#include <stm32f10x.h>

void delay(uint32_t ticks) {
    for (uint32_t i = 0; i < ticks; i++) {
        __NOP();
    }
}

int __attribute((noreturn)) main(void) {
    // Включаем тактирование для AFIO
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
    // Включаем тактирование портов C и A
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPAEN;

    // PC13 как выход push-pull
    // MODE13 = 01 (output 10 MHz), CNF13 = 00 (general purpose push-pull)
    GPIOC->CRH = GPIOC->CRH & ~(GPIO_CRH_CNF13 | GPIO_CRH_MODE13) | GPIO_CRH_MODE13_0;

    // PA0 как вход с pull-up/down
    // MODE0 = 00 (input), CNF0 = 10 (input with pull-up/down)
    GPIOA->CRL = GPIOA->CRL & ~(GPIO_CRL_CNF0 | GPIO_CRL_MODE0) | GPIO_CRL_CNF0_1;

    // --- PA1 как вход с pull-up/down ---
    GPIOA->CRL = GPIOA->CRL & ~(GPIO_CRL_CNF1 | GPIO_CRL_MODE1) | GPIO_CRL_CNF1_1;

    // Включаем подтяжку вверх (pull-up) для PA0 и PA1
    GPIOA->ODR |= GPIO_ODR_ODR0 | GPIO_ODR_ODR1;


    // 0 -> 1/64 Гц, 6 -> 1 Гц, 12 -> 64 Гц
    uint8_t freq_step = 6; // начинаем с 1 Гц
    const uint8_t MIN_STEP = 0;
    const uint8_t MAX_STEP = 12;

    // Таблица задержек (половина периода) для частот 1/64 .. 64 Гц
    const uint32_t delays[13] = {
        64000000U,
        32000000U,
        16000000U,
        8000000U,
        4000000U,
        2000000U,
        1000000U,
        500000U,
        250000U,
        125000U,
        62500U,
        31250U,
        15625U
    };
	// начинаем с 1 Гц (index = 6)
    uint8_t freq_index = 6;

    while (1) {
        int a_pressed = !(GPIOA->IDR & GPIO_IDR_IDR0); // Кнопка A (вверх)
        int c_pressed = !(GPIOA->IDR & GPIO_IDR_IDR1); // Кнопка C (вниз)

        // Кнопка A: увеличиваем частоту
        if (a_pressed) {
            if (freq_index < 12) {
                freq_index++;
            }
            // Ждём отпускания
            while (!(GPIOA->IDR & GPIO_IDR_IDR0));
            // Антидребезг
            delay(50000);
        }

        // Кнопка C: уменьшаем частоту
        if (c_pressed) {
            if (freq_index > 0) {
                freq_index--;
            }
            while (!(GPIOA->IDR & GPIO_IDR_IDR1));
            delay(50000);
        }

		uint32_t delay_ticks = delays[freq_index];
		GPIOC->ODR &= ~GPIO_ODR_ODR13; // LED ON
        delay(delay_ticks);
		GPIOC->ODR |= GPIO_ODR_ODR13;  // LED OFF
        delay(delay_ticks);
    }
}
