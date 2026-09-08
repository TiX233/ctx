#ifndef MAIN_CORO_H_
#define MAIN_CORO_H_

// Auto-generated private data structures for async coroutines

struct _coval_task_1 {
    // 参数
    // (无)

    // 需要持久化的局部变量
    // (无)

    // 返回值
    int _coretval_;
};

struct coro_stu* _co_task_1(struct coro_stu *father, struct coro_stu *co);

struct _coval_task_2 {
    // 参数
    // (无)

    // 需要持久化的局部变量
    uint8_t flag_is_time_out;

    // 返回值
    int _coretval_;
};

struct coro_stu* _co_task_2(struct coro_stu *father, struct coro_stu *co);

struct _coval_task_3 {
    // 参数
    // (无)

    // 需要持久化的局部变量
    uint8_t flag_is_time_out;

    // 返回值
    int _coretval_;
};

struct coro_stu* _co_task_3(struct coro_stu *father, struct coro_stu *co);

#endif // MAIN_CORO_H_
