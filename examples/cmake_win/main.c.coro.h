#ifndef MAIN_CORO_H_
#define MAIN_CORO_H_

// Auto-generated private data structures for async coroutines

struct _coval_task_wait_events {
    // 参数
    int task_id;
    uint32_t events_wait_for;
    TickType_t time_out;
    uint8_t and_or;

    // 需要持久化的局部变量
    // (无)

    // 返回值
    int _coretval_;
};

void _co_task_wait_events(struct coro_stu *father, struct coro_stu *co, int task_id, uint32_t events_wait_for, TickType_t time_out, uint8_t and_or);

struct _coval_task_events_publisher {
    // 参数
    // (无)

    // 需要持久化的局部变量
    // (无)

    // 返回值
    int _coretval_;
};

void _co_task_events_publisher(struct coro_stu *father, struct coro_stu *co);

#endif // MAIN_CORO_H_
