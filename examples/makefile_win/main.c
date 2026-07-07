#include "main.h"

#include "ctx.h"

#include "main.c.coro.h"

// ---------- 模拟中断开关 ----------
CRITICAL_SECTION cs;                        // 模拟中断锁

// systick 服务函数
DWORD WINAPI systick_thread(LPVOID param);

_async void task_test(TickType_t delay, int task_id){
    while(1){
        printf("Task %d running...\n", task_id);
        _await delay_ticks(delay);
    }
}

// ---------- 主线程（调度器 + 任务执行） ----------
int main(void) {
    // 初始化临界区
    InitializeCriticalSection(&cs);

    // 创建 systick 中断，以高优先级线程形式创建
    HANDLE hTickThread = CreateThread(NULL, 0, systick_thread, NULL, 0, NULL);
    SetThreadPriority(hTickThread, THREAD_PRIORITY_TIME_CRITICAL); // 优先级比 main 高

    // 初始化内存池
    ctx_mem_pool_init();

    // 启动异步函数
    _start_async(NULL, task_test, 10, 1);
    _start_async(NULL, task_test, 100, 2);
    _start_async(NULL, task_test, 500, 3);

    // 运行调度器
    ltx_Sys_scheduler();
    // 后续代码不会被执行

    while(1);

    CloseHandle(hTickThread);
    DeleteCriticalSection(&cs);
    return 0;
}

// 模拟 systick 定时中断
DWORD WINAPI systick_thread(LPVOID param){
    (void)param;
    while (1) {
        Sleep(10);                          // 10 ms 一次

        ltx_Sys_tick_tack();
    }
    return 0;
}

#include "main.c.coro"
