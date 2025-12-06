#include <stdint.h>
#include <stm32f10x.h>


// Границы предделителя
#define PSC_MIN  1
#define PSC_MAX  65535
// Начальное значение предделителя (примерно "средняя" скорость)
#define PSC_START 1023
#define ARR_VALUE 4095


void delay(uint32_t ticks) {
    for (uint32_t i = 0; i < ticks; i++) {
        __NOP();
    }
}

 
// ---------------------- Инициализация таймера TIM2 ----------------------
static void TIM2_Init(void)
{
    // Включаем тактирование TIM2
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    // Сбрасываем TIM2
    RCC->APB1RSTR |=  RCC_APB1RSTR_TIM2RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_TIM2RST;

    // Настраиваем предделитель и авто-перезагрузку
    TIM2->PSC = PSC_START;  // предделитель
    TIM2->ARR = ARR_VALUE;  // предел счётчика

    // Разрешаем прерывание по событию обновления (UIF)
    TIM2->DIER |= TIM_DIER_UIE;

    // Разрешаем прерывание TIM2 в NVIC
    NVIC_ClearPendingIRQ(TIM2_IRQn);
    NVIC_EnableIRQ(TIM2_IRQn);

    // Включаем таймер
    TIM2->CR1 |= TIM_CR1_CEN;
}

// ---------------------- Обработчик прерывания TIM2 ----------------------
void TIM2_IRQHandler(void)
{
    // Проверяем флаг обновления
    if (TIM2->SR & TIM_SR_UIF)
    {
        // Переключаем светодиод на PC13
        // (прочитали ODR и инвертировали бит 13)
        GPIOC->ODR ^= GPIO_ODR_ODR13;

        // Сбрасываем флаг прерывания
        TIM2->SR &= ~TIM_SR_UIF;
    }
}

// ------------------------------ MAIN ------------------------------------
int __attribute((noreturn)) main(void) {
    // Включаем тактирование для AFIO
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
    // Включаем тактирование портов C и A
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPAEN;

    // PC13 как выход push-pull (светодиод)
    GPIOC->CRH = (GPIOC->CRH & ~(GPIO_CRH_CNF13 | GPIO_CRH_MODE13)) | GPIO_CRH_MODE13_0;

    // PA0 как вход с pull-up/down (кнопка A)
    GPIOA->CRL = (GPIOA->CRL & ~(GPIO_CRL_CNF0 | GPIO_CRL_MODE0)) | GPIO_CRL_CNF0_1;

    // PA1 как вход с pull-up/down (кнопка C)
    GPIOA->CRL = (GPIOA->CRL & ~(GPIO_CRL_CNF1 | GPIO_CRL_MODE1)) | GPIO_CRL_CNF1_1;

    // Включаем подтяжку вверх (pull-up) для PA0 и PA1
    GPIOA->ODR |= GPIO_ODR_ODR0 | GPIO_ODR_ODR1;

    // Инициализируем таймер TIM2
    TIM2_Init();

    while (1) {
        // Чтение кнопок (активный уровень 0, т.к. pull-up)
        int a_pressed = !(GPIOA->IDR & GPIO_IDR_IDR0);  // кнопка A: "быстрее"
        int c_pressed = !(GPIOA->IDR & GPIO_IDR_IDR1);  // кнопка C: "медленнее"

        // Кнопка A: увеличить частоту мигания (уменьшить PSC)
        if (a_pressed) {
            uint32_t psc = TIM2->PSC;
            if (psc > PSC_MIN) {
                // Увеличивает частоту
                TIM2->PSC = (uint16_t)(psc >> 1);
            }
            // Ждём отпускания кнопки + антидребезг
            while (!(GPIOA->IDR & GPIO_IDR_IDR0));
            delay(50000);
        }

        // Кнопка C: уменьшить частоту мигания (увеличить PSC)
        if (c_pressed) {
            uint32_t psc = TIM2->PSC;
            if (psc < PSC_MAX) {
                // Уменьшает частоту
                TIM2->PSC = (uint16_t)(psc << 1);
            }
            while (!(GPIOA->IDR & GPIO_IDR_IDR1));
            delay(50000);
        }
    }
}
