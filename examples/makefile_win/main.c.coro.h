#ifndef MAIN_CORO_H_
#define MAIN_CORO_H_

// Auto-generated private data structures for async coroutines

struct _coval_task_test {
    // 参数
    TickType_t delay;
    int task_id;

    // 需要持久化的局部变量
    // (无)

    // 返回值
    int _coretval_;
};

void _co_task_test(struct coro_stu *father, struct coro_stu *co, TickType_t delay, int task_id);

#endif // MAIN_CORO_H_
