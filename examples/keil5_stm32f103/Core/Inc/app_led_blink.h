#ifndef __app_led_blink_H__
#define __app_led_blink_H__

#include "ctx.h"

#include "app_led_blink.c.coro.h"

// 指定某个 led 闪烁与次数
void led_blink(uint8_t led_id, TickType_t high_level_ticks, uint32_t times);

#endif // __app_led_blink_H__
