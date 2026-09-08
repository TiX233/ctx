#include "main.h"

#include "ltx.h"

#include "ctx.h"
#include "ctx_mutex.h"

#include "main.c.coro.h"

// 模拟 systick 服务函数
DWORD WINAPI systick_thread(LPVOID param);

// 测试用互斥锁
struct ctx_mutex_stu mutex_test;

_async void task_1(void){
    
    _var_local uint8_t flag_is_time_out;

    printf("task1 running...(%d)\n", ltx_Sys_get_tick());

    // 超时时间 100 ticks
    while(1){
        flag_is_time_out = _await ctx_mutex_take(&mutex_test, 100);
        if(!flag_is_time_out){
            break;
        }
        printf("task1 take mutex timeout, retry...(%d)\n", ltx_Sys_get_tick());
    }
    
    printf("task1 take mutex SUCCESS.(%d)\n", ltx_Sys_get_tick());
    _await delay_ticks(500);
    printf("task1 give mutex.(%d)\n", ltx_Sys_get_tick());
    _await ctx_mutex_give(&mutex_test);
    printf("task1 exit.(%d)\n", ltx_Sys_get_tick());
}

_async void task_2(void){
    
    uint8_t flag_is_time_out;

    _await delay_ticks(10);
    printf("task2 running...(%d)\n", ltx_Sys_get_tick());

    // 超时时间 100 ticks
    while(1){
        flag_is_time_out = _await ctx_mutex_take(&mutex_test, 100);
        if(!flag_is_time_out){
            break;
        }
        printf("task2 take mutex timeout, retry...(%d)\n", ltx_Sys_get_tick());
    }
    
    printf("task2 take mutex SUCCESS.(%d)\n", ltx_Sys_get_tick());
    _await delay_ticks(500);
    printf("task2 give mutex.(%d)\n", ltx_Sys_get_tick());
    _await ctx_mutex_give(&mutex_test);
    printf("task2 exit.(%d)\n", ltx_Sys_get_tick());
}

_async void task_3(void){
    
    uint8_t flag_is_time_out;

    _await delay_ticks(20);
    printf("task3 running...(%d)\n", ltx_Sys_get_tick());

    // 超时时间 100 ticks
    while(1){
        flag_is_time_out = _await ctx_mutex_take(&mutex_test, 100);
        if(!flag_is_time_out){
            break;
        }
        printf("task3 take mutex timeout, retry...(%d)\n", ltx_Sys_get_tick());
    }
    
    printf("task3 take mutex SUCCESS.(%d)\n", ltx_Sys_get_tick());
    _await delay_ticks(500);
    printf("task3 give mutex.(%d)\n", ltx_Sys_get_tick());
    _await ctx_mutex_give(&mutex_test);
    printf("task3 try take again.(%d)\n", ltx_Sys_get_tick());

    flag_is_time_out = _await ctx_mutex_take(&mutex_test, 100);
    if(!flag_is_time_out){
        
        printf("task3 take mutex SUCCESS.(%d)\n", ltx_Sys_get_tick());

        printf("task3 give mutex.(%d)\n", ltx_Sys_get_tick());
        _await ctx_mutex_give(&mutex_test);
        printf("task3 exit.(%d)\n", ltx_Sys_get_tick());
        return ;
    }
    printf("task3 take mutex timeout, retry...(%d)\n", ltx_Sys_get_tick());
}

struct coro_stu *co2;
struct coro_stu *co3;

// ---------- 主线程（调度器 + 任务执行） ----------
int main(void) {
    // 初始化临界区
    InitializeCriticalSection(&__g_spin_lock);

    // 创建 systick 中断，以高优先级线程形式创建
    HANDLE hTickThread = CreateThread(NULL, 0, systick_thread, NULL, 0, NULL);
    SetThreadPriority(hTickThread, THREAD_PRIORITY_TIME_CRITICAL); // 优先级比 main 高

    // 初始化内存池
    ctx_mem_pool_init();

    // 启动异步任务
    _start_async(NULL, task_1);
    co2 = _start_async(NULL, task_2);
    co3 = _start_async(NULL, task_3);

    // 运行调度器
    ltx_Sys_scheduler(0);
    // 后续代码不会被执行

    while(1);

    CloseHandle(hTickThread);
    DeleteCriticalSection(&__g_spin_lock);
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
