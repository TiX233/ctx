#include "main.h"

#include "ltx.h"

#include "ctx.h"
#include "ctx_events.h"

#include "main.c.coro.h"

// 需要注意，新版为了不给事件组额外创建任务不动态分配内存，事件与 的返回值将会是还未触发的事件而不是已触发的事件！
// 例如 与等待 1011，但是只触发了 0110，那么返回值将会是 1000 0000 0000 0000 0000 0000 0000 1001（最高位为 1 代表超时）
// 或等待则保持返回已触发的事件
// 例如 或等待 1011，触发了 0110，那么返回值将会是 0010

// 测试用事件组，供多个消费者使用
struct ctx_events_stu events_test;

// 等待事件组的消费者
_async void task_wait_events(int task_id, uint32_t events_wait_for, TickType_t time_out, ctx_events_type_e and_or){
    
    printf("Task %d start, wait for 0x%08X(%s) in %d ticks.\n", task_id, events_wait_for, and_or?"AND":"OR", time_out);
    uint32_t events_get = _await ctx_wait_events(&events_test, time_out, events_wait_for, and_or);
    if(ctx_Events_is_timeout(events_get)){
        // 超时
        if(and_or == CTX_EVENTS_TYPE_OR)
            printf("Task %d wait events timeout, get events: 0x%08X\n", task_id, events_get);
        else
            printf("Task %d wait events timeout, get events: 0x%08X\n", task_id, events_wait_for & (~events_get));
    }else {
        if(and_or == CTX_EVENTS_TYPE_OR)
            printf("Task %d wait events Okay, get events: 0x%08X\n", task_id, events_get);
        else
            printf("Task %d wait events Okay, get events: 0x%08X\n", task_id, events_wait_for & (~events_get));
    }
}

// 发布事件的生产者
_async void task_events_publisher(void){
    // 两秒后发布 0b0001
    _await delay_ticks(2000);
    printf("    publish 0b0001\n");
    ctx_Events_publish(&events_test, 0x0001);

    // 再过两秒发布 0b0110
    _await delay_ticks(2000);
    printf("    publish 0b0110\n");
    ctx_Events_publish(&events_test, 0x0006);
}

// ---------- 主线程（调度器 + 任务执行） ----------
int main(void) {
    // 初始化临界区
    InitializeCriticalSection(&__g_spin_lock);

    // 初始化内存池
    ctx_mem_pool_init();

    // 启动异步函数
    // 启动消费者
    _start_async(NULL, task_wait_events, 1, 0x0001, 5000, CTX_EVENTS_TYPE_AND); // 与等待 0b0001，5 秒超时时间
    _start_async(NULL, task_wait_events, 2, 0x0003, 5000, CTX_EVENTS_TYPE_AND); // 与等待 0b0011，5 秒超时时间
    _start_async(NULL, task_wait_events, 3, 0x000c, 5000, CTX_EVENTS_TYPE_OR);  // 或等待 0b1100，5 秒超时时间
    _start_async(NULL, task_wait_events, 4, 0x0009, 5000, CTX_EVENTS_TYPE_AND); // 与等待 0b1001，5 秒超时时间

    // 启动生产者
    _start_async(NULL, task_events_publisher);

    // 运行调度器
    ltx_Sys_scheduler(0);
    // 后续代码不会被执行

    while(1);

    DeleteCriticalSection(&__g_spin_lock);
    return 0;
}

#include "main.c.coro"
