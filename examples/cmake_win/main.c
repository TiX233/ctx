#include "main.h"

#include "ctx.h"
#include "ctx_events.h"

#include "main.c.coro.h"

// ---------- 模拟中断开关 ----------
CRITICAL_SECTION cs;                        // 模拟中断锁

// systick 服务函数
DWORD WINAPI systick_thread(LPVOID param);

// 测试用事件组，供多个消费者使用
struct ctx_events_stu events_test;

// 等待事件组的消费者
_async void task_wait_events(int task_id, uint32_t events_wait_for, TickType_t time_out, uint8_t and_or){
    
    printf("Task %d start, wait for 0x%08X(%s) in %d ticks.\n", task_id, events_wait_for, and_or?"OR":"AND", time_out);
    uint32_t events_get = _await ctx_wait_events(&events_test, time_out, events_wait_for, and_or);
    if(ctx_Events_is_timeout(events_get)){
        // 超时
        printf("Task %d wait events timeout, now events: 0x%08X\n", task_id, events_get);
    }else {
        printf("Task %d wait events Okay: 0x%08X\n", task_id, events_get);
    }
}

// 发布事件的生产者
_async void task_events_publisher(void){
    // 两秒后发布 0b0001
    _await delay_ticks(200);
    printf("    publish 0b0001\n");
    ctx_Events_publish(&events_test, 0x0001);

    // 再过两秒发布 0b0110
    _await delay_ticks(200);
    printf("    publish 0b0110\n");
    ctx_Events_publish(&events_test, 0x0006);
}

// ---------- 主线程（调度器 + 任务执行） ----------
int main(void) {
    // 初始化临界区
    InitializeCriticalSection(&cs);

    // 创建 systick 中断，以高优先级线程形式创建
    HANDLE hTickThread = CreateThread(NULL, 0, systick_thread, NULL, 0, NULL);
    SetThreadPriority(hTickThread, THREAD_PRIORITY_TIME_CRITICAL); // 优先级比 main 高

    // 初始化事件组
    ctx_Events_init(&events_test);

    // 初始化内存池
    ctx_mem_pool_init();

    // 启动异步函数
    // 启动消费者
    _start_async(NULL, task_wait_events, 1, 0x0001, 500, CTX_EVENTS_TYPE_AND); // 与等待 0b0001，5 秒超时时间
    _start_async(NULL, task_wait_events, 2, 0x0003, 500, CTX_EVENTS_TYPE_AND); // 与等待 0b0011，5 秒超时时间
    _start_async(NULL, task_wait_events, 3, 0x000c, 500, CTX_EVENTS_TYPE_OR);  // 或等待 0b1100，5 秒超时时间
    _start_async(NULL, task_wait_events, 4, 0x0009, 500, CTX_EVENTS_TYPE_AND); // 与等待 0b1001，5 秒超时时间

    // 启动生产者
    _start_async(NULL, task_events_publisher);

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
