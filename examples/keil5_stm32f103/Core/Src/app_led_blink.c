#include "app_led_blink.h"

#include "main.h"

struct {
    GPIO_TypeDef *GPIOx;
    uint16_t GPIO_Pin;
} led_pins[] = {
    [0] = {
        .GPIOx = GPIOB,
        .GPIO_Pin = GPIO_PIN_7,
    },
    [1] = {
        .GPIOx = GPIOB,
        .GPIO_Pin = GPIO_PIN_8,
    },
    [2] = {
        .GPIOx = GPIOB,
        .GPIO_Pin = GPIO_PIN_9,
    },
};

void led_set(uint8_t led_id, uint8_t state){
    HAL_GPIO_WritePin(led_pins[led_id].GPIOx, led_pins[led_id].GPIO_Pin, !state);
}

// 指定某个 led 闪烁一定次数，并且可指定电平保持时间
_async void led_blink(uint8_t led_id, TickType_t high_level_ticks, uint32_t times){
    for(uint32_t i = 0; i < times; i ++){
        led_set(led_id, 1);
        _await delay_ticks(high_level_ticks);
        led_set(led_id, 0);
        _await delay_ticks(high_level_ticks);
    }
}

#include "app_led_blink.c.coro"
