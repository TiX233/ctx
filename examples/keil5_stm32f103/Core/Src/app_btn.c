#include "app_btn.h"
#include "ltx.h"
#include "app_led_blink.h"

struct ltx_Topic_stu topic_btn_click = _LTX_TOPIC_DEAFULT_CONFIG(topic_btn_click);

_async void btn_check(void){

    while(1){
        // 以最大时限等待按键单击事件
        uint8_t flag_is_wait_topic_timeout = _await wait_topic(&topic_btn_click, -1);
       
        if(flag_is_wait_topic_timeout){ // 如果等待按键单击事件超时
            // 快速闪烁十次 led2，动态创建
            _await led_blink(2, 100, 10);
        }else { // 在时限内发生了按键单机时间
            // 快速闪烁三次 led2，动态创建
            _await led_blink(2, 150, 3);
        }
    }
}

// 按键消抖完毕闹钟
void alarm_cb_btn_debounce(void *param){
    // LTX_LOG_INFO("Alarm ring: %d\n", ltx_Sys_get_tick());
    if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_12) == 0){
        // 下降沿才会发布按键单击事件
        ltx_Topic_publish(&topic_btn_click);
    }
}
struct ltx_Topic_subscriber_stu alarm_btn_debounce_subscriber;
struct ltx_Alarm_stu alarm_btn_debounce = {
    .diff_tick = 0,
    .topic = {
        .flag_is_pending = 0,
        .subscriber_head = {
            .prev = NULL,
            .next = &alarm_btn_debounce_subscriber
        },
        .subscriber_tail = &alarm_btn_debounce_subscriber,
        .next = NULL
    },
    .prev = NULL,
    .next = NULL
};
struct ltx_Topic_subscriber_stu alarm_btn_debounce_subscriber = {
    .callback_func = alarm_cb_btn_debounce,
    .prev = &(alarm_btn_debounce.topic.subscriber_head),
    .next = NULL,
};


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
    // 重置 15ms 消抖闹钟
    ltx_Alarm_add(&alarm_btn_debounce, 15);
}

#include "app_btn.c.coro"
