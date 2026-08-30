#ifndef APP_LED_BLINK_CORO_H_
#define APP_LED_BLINK_CORO_H_

// Auto-generated private data structures for async coroutines

struct _coval_led_blink {
    // 参数
    uint8_t led_id;
    TickType_t high_level_ticks;
    uint32_t times;

    // 需要持久化的局部变量
    uint32_t i;

    // 返回值
    int _coretval_;
};

struct coro_stu* _co_led_blink(struct coro_stu *father, struct coro_stu *co, uint8_t led_id, TickType_t high_level_ticks, uint32_t times);

#endif // APP_LED_BLINK_CORO_H_
