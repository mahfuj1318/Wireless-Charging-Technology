#include <stdint.h>
#include "BInit.h"
#include "isr.h"
#include "display.h"

#define MASK(x) (1UL << (x))
#define PWM_PERIOD (24000)
#define FULL_ON (PWM_PERIOD-1)
#define FULL_OFF (0)
#define Pin_Enable (1) // on port D1
#define Pin_A1 (8)     // on port B11
#define Pin_A2 (9)     // on port B10

void Init_PINS() {
    SIM->SCGC5 |= SIM_SCGC5_PORTA_MASK | SIM_SCGC5_PORTB_MASK;
    PORTB->PCR[Pin_A1] &= ~PORT_PCR_MUX_MASK;
    PORTB->PCR[Pin_A1] |= (1UL << 8);
    PORTB->PCR[Pin_A2] &= ~PORT_PCR_MUX_MASK;
    PORTB->PCR[Pin_A2] |= (1UL << 8);
    PTB->PDDR |= MASK(Pin_A1);
    PTB->PDDR |= MASK(Pin_A2);
    PTB->PCOR = MASK(Pin_A1);
    PTB->PSOR = MASK(Pin_A2);
}

#define DELAY(flag) { while (!flag) {;} flag = 0; }

int main() {
    static uint8_t time300ms_cnt = 0;
    bsp_init();
    start_lcd();
    DELAY(flag_1sec);
    lcd_number_write("Wireless ", LCD_LINE1, UNSCROLL);
    lcd_number_write("Charger", LCD_LINE2, UNSCROLL);
    DELAY(flag_1sec);
    DELAY(flag_1sec);
    Init_PINS();

    while (1) {
        PTB->PTOR |= MASK(Pin_A1);
        PTB->PTOR |= MASK(Pin_A2);
        for (int i = 0; i < 50; i++);
    }

    for (;;) {
        if (flag_100msec) {
            flag_100msec = 0U;
            time300ms_cnt++;
            if (time300ms_cnt >= 3) {
                time300ms_cnt = 0;
                lcd_number_write("Charging. ", LCD_LINE2, SCROLL);
            }
        }
        if (flag_1sec) {
            time_update_lcd();
            flag_1sec = 0U;
        }
    }
}

typedef struct {
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
} time_str;

static void lcd_command(uint8_t command);
static void lcd_writee(uint8_t nibble);
static void delay(uint16_t cnt);
static uint8_t scroll_number_stored;
static char lcd_scroll_msg[50];

void start_lcd(void) {
    lcd_command(0x02);
    lcd_command(0x28);
    lcd_command(0x0C);
    lcd_command(0x01);
}

void lcd_number_write(char *number, lcd_line line, lcd_scrolling scroll_type) {
    uint8_t char_written;
    char *temp = " ";
    char *pscroll_msg;
    uint8_t i;
    char tmp;
    static uint8_t scroll_chars;

    if (line == LCD_LINE1) {
        lcd_command(0x80);
    } else {
        lcd_command(0xC0);
    }

    if ((scroll_type == UNSCROLL) || (!scroll_number_stored)) {
        char_written = lcd_string_write(&number);

        if (scroll_type == SCROLL) {
            scroll_chars = char_written;
            while (*number && scroll_chars < 49) {
                lcd_scroll_msg[scroll_chars] = *number++;
                scroll_chars++;
            }
            lcd_scroll_msg[scroll_chars] = '\0';
            scroll_number_stored = 1;
        } else {
            while (char_written < 16) {
                lcd_string_write(&temp);
                char_written++;
            }
        }
    } else {
        tmp = lcd_scroll_msg[0];
        for (i = 0; i < scroll_chars; i++) {
            lcd_scroll_msg[i] = lcd_scroll_msg[i + 1];
        }
        lcd_scroll_msg[scroll_chars - 1] = tmp;
        pscroll_msg = lcd_scroll_msg;
        (void)lcd_string_write(&pscroll_msg);
    }
}

uint8_t lcd_string_write(char **str) {
    uint8_t cnt = 0;
    while (**str && cnt < 16) {
        if (!scroll_number_stored) {
            lcd_scroll_msg[cnt] = **str;
        }
        GPIOC->PDOR |= LCD_RS;
        GPIOC->PDOR &= ~LCD_RW;
        lcd_writee(**str & 0xF0);
        GPIOC->PDOR |= LCD_E;
        delay(10000);
        GPIOC->PDOR &= ~LCD_E;
        delay(10000);
        lcd_writee((**str << 4) & 0xF0);
        GPIOC->PDOR |= LCD_E;
        delay(10000);
        GPIOC->PDOR &= ~LCD_E;
        delay(10000);
        (*str)++;
        cnt++;
    }
    return cnt;
}

void lcd_byte_write(uint8_t input, uint8_t len) {
    uint8_t byte[3];
    uint8_t i;

    if (len == 1) {
        byte[0] = input;
    } else if (len == 2) {
        byte[0] = input / 10;
        byte[1] = input % 10;
    } else {
        byte[0] = input / 100;
        byte[1] = (input / 10) % 10;
        byte[2] = input % 10;
    }

    for (i = 0; i < len; i++) {
        GPIOC->PDOR |= LCD_RS;
        GPIOC->PDOR &= ~LCD_RW;
        lcd_writee(('0' + byte[i]) & 0xF0);
        GPIOC->PDOR |= LCD_E;
        delay(10000);
        GPIOC->PDOR &= ~LCD_E;
        delay(10000);
        lcd_writee((('0' + byte[i]) << 4) & 0xF0);
        GPIOC->PDOR |= LCD_E;
        delay(10000);
        GPIOC->PDOR &= ~LCD_E;
        delay(10000);
    }
}

static void lcd_command(uint8_t command) {
    GPIOC->PDOR &= ~LCD_RS;
    GPIOC->PDOR &= ~LCD_RW;
    lcd_writee(command & 0xF0);
    GPIOC->PDOR |= LCD_E;
    delay(10000);
    GPIOC->PDOR &= ~LCD_E;
    delay(10000);
    lcd_writee((command << 4) & 0xF0);
    GPIOC->PDOR |= LCD_E;
    delay(10000);
    GPIOC->PDOR &= ~LCD_E;
    delay(10000);
}

static void lcd_writee(uint8_t nibble) {
    uint32_t gpio_temp = GPIOC->PDOR;
    if (nibble & 0x80) gpio_temp |= lcd_pin7; else gpio_temp &= ~lcd_pin7;
    if (nibble & 0x40) gpio_temp |= lcd_pin6; else gpio_temp &= ~lcd_pin6;
    if (nibble & 0x20) gpio_temp |= lcd_pin5; else gpio_temp &= ~lcd_pin5;
    if (nibble & 0x10) gpio_temp |= lcd_pin4; else gpio_temp &= ~lcd_pin4;
    GPIOC->PDOR = gpio_temp;
}

static void delay(int num) {
    int i, j;
    for (j = 0; j < 5; j++) {
        for (i = 0; i < num; i++);
    }
}
