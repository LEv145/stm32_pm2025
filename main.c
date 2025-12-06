#include <stdint.h>
#include <stm32f10x.h>


// ==================== ПРОСТАЯ ЗАДЕРЖКА ====================
static void delay(volatile uint32_t t)
{
    while (t--)
    {
        __NOP();
    }
}


// ==================== НАСТРОЙКА SPI1 ====================
//
// PA5 - SCK  (SPI1_SCK)
// PA6 - MISO (SPI1_MISO)
// PA7 - MOSI (SPI1_MOSI)
//
void SPI1_Init(void)
{
    // Включаем тактирование SPI1 и порта A
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    // --- Настраиваем пины SPI1 ---

    // PA5 (SCK) - Альтернативная функция, push-pull, 2 МГц
    GPIOA->CRL &= ~(GPIO_CRL_MODE5 | GPIO_CRL_CNF5);
    GPIOA->CRL |=  (GPIO_CRL_MODE5_0 | GPIO_CRL_MODE5_1);
    GPIOA->CRL |=  (GPIO_CRL_CNF5_1);

    // PA6 (MISO) - Вход, плавающий
    GPIOA->CRL &= ~(GPIO_CRL_MODE6 | GPIO_CRL_CNF6);
    GPIOA->CRL |=  (GPIO_CRL_CNF6_0);

    // PA7 (MOSI) - Альтернативная функция, push-pull, 2 МГц
    GPIOA->CRL &= ~(GPIO_CRL_MODE7 | GPIO_CRL_CNF7);
    GPIOA->CRL |=  (GPIO_CRL_MODE7_0 | GPIO_CRL_MODE7_1);
    GPIOA->CRL |=  (GPIO_CRL_CNF7_1);

    // --- Настройка SPI1 ---
    //
    // CPOL = 1, CPHA = 1
    // MSTR = 1 (мастер)
    // BR   = 111 (fPCLK/256)
    // SSM  = 1 (программное управление CS)
    //
    SPI1->CR1 = 0;
    SPI1->CR1 |= SPI_CR1_CPOL | SPI_CR1_CPHA;
    SPI1->CR1 |= SPI_CR1_MSTR;
    SPI1->CR1 |= SPI_CR1_BR;
    SPI1->CR1 |= SPI_CR1_SSM | SPI_CR1_SSI;

    // Включаем SPI1
    SPI1->CR1 |= SPI_CR1_SPE;
}

// Отправка байта по SPI1
void SPI1_Write(uint8_t data)
{
    // Ждем, пока не освободится буфер передатчика
    while (!(SPI1->SR & SPI_SR_TXE)) {}

    // Заполняем буфер передатчика
    SPI1->DR = data;

    // Ждем окончания передачи
    while (SPI1->SR & SPI_SR_BSY) {}
}

// Приём байта по SPI1
uint8_t SPI1_Read(void)
{
    // Запускаем обмен
    SPI1->DR = 0x00;

    // Ждем, пока не появится новое значение в буфере приемника
    while (!(SPI1->SR & SPI_SR_RXNE)) {}

    // Возвращаем значение буфера приемника
    return (uint8_t)SPI1->DR;
}

// =================== ДИСПЛЕЙ SSD1306 ===================
//
// PA1 - D/C  (0 = команда, 1 = данные)
// PA4 - /CS  (0 = активно)
// PA0 - /RES
//


#define DISP_RES  GPIO_ODR_ODR0
#define DISP_DC   GPIO_ODR_ODR1
#define DISP_CS   GPIO_ODR_ODR4


// Отправка КОМАНДЫ в дисплей
void display_cmd(uint8_t cmd)
{
    GPIOA->ODR &= ~DISP_CS;  // CS = 0 (активный)
    GPIOA->ODR &= ~DISP_DC;  // DC = 0 (команда)
    delay(1000);
    SPI1_Write(cmd);
    GPIOA->ODR |=  DISP_CS;  // CS = 1 (отпустить дисплей)
}

// Отправка ДАННЫХ в дисплей
void display_data(uint8_t data)
{
    GPIOA->ODR &= ~DISP_CS;  // CS = 0
    GPIOA->ODR |=  DISP_DC;  // DC = 1 (данные)
    delay(1000);
    SPI1_Write(data);
    GPIOA->ODR |=  DISP_CS;  // CS = 1
}


// Рестарт дисплея
void display_reset(void)
{
    // /RES = 0 (активный сброс)
    GPIOA->ODR &= ~DISP_RES;
    delay(100000);

    // /RES = 1 (нормальная работа)
    GPIOA->ODR |= DISP_RES;
    delay(100000);
}


// Включаем горизонтальный режим адресации RAM
void display_set_horizontal_mode(void)
{
    display_cmd(0x20); // команда "Memory addressing mode"
    display_cmd(0x00); // 0x00 = горизонтальный режим
}


// ================= РИСОВАНИЕ ШАХМАТНОЙ ДОСКИ =================
//
// Экран 128x64:
//  - 8 "страниц" по вертикали (каждая страница = 8 пикселей)
//  - 128 столбцов.
//
void draw_chessboard(void)
{
    uint8_t page;
    uint16_t x;

    display_set_horizontal_mode();

    for (page = 0; page < 8; page++) 
    {
        for (x = 0; x < 128; x++)
        {
            uint8_t cell_x = x / 16;
            uint8_t cell_y = page;

            uint8_t color = ((cell_x + cell_y) & 1);
            uint8_t byte  = color ? 0xFF : 0x00;

            display_data(byte);
        }
    }
}


int __attribute__((noreturn)) main(void)
{
    // Тактирование порта A (для DC/CS)
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    // PA0 (RES), PA1 (DC) и PA4 (CS) как выходы 2 МГц, push-pull
    GPIOA->CRL &= ~(GPIO_CRL_MODE0 | GPIO_CRL_CNF0 |
                    GPIO_CRL_MODE1 | GPIO_CRL_CNF1 |
                    GPIO_CRL_MODE4 | GPIO_CRL_CNF4);

    GPIOA->CRL |= GPIO_CRL_MODE0_1;   // PA0: 2 МГц
    GPIOA->CRL |= GPIO_CRL_MODE1_1;   // PA1: 2 МГц
    GPIOA->CRL |= GPIO_CRL_MODE4_1;   // PA4: 2 МГц

    // Начальное состояние: RES=1, DC=0, CS=1
    GPIOA->ODR |=  DISP_RES;   // /RES высоко (не в сбросе)
    GPIOA->ODR &= ~DISP_DC;    // командный режим
    GPIOA->ODR |=  DISP_CS;    // дисплей не выбран

    // Инициализация SPI1 (часть "библиотеки")
    SPI1_Init();
    
    // Аппаратный сброс дисплея
    display_reset();

    // Пример команды включения дисплея
    uint8_t display_on = 1;
    display_cmd(0xAE | display_on);  // 0xAF = Display ON

    draw_chessboard();

    while (1) {}
}