#include "main.h"

#include "ltx.h"

#include "ctx.h"
#include "ctx_mutex.h"

#include "main.c.coro.h"

#ifdef ltx_cfg_USE_IDLE_SLEEP
    HANDLE g_hSemaphore = NULL;
#endif

// 测试用互斥锁
struct ctx_mutex_stu mutex_test;

_async void task_1(void){
    
    _var_local uint8_t flag_is_time_out;

    printf("task1 running...(%d)\n", ltx_Sys_get_tick());

    // 超时时间 1000 ticks
    while(1){
        flag_is_time_out = _await ctx_mutex_take(&mutex_test, 1000);
        if(!flag_is_time_out){
            break;
        }
        printf("task1 take mutex timeout, retry...(%d)\n", ltx_Sys_get_tick());
    }
    
    printf("task1 take mutex SUCCESS.(%d)\n", ltx_Sys_get_tick());
    _await delay_ticks(5000);
    printf("task1 give mutex.(%d)\n", ltx_Sys_get_tick());
    _await ctx_mutex_give(&mutex_test);
    printf("task1 exit.(%d)\n", ltx_Sys_get_tick());
}

_async void task_2(void){
    
    uint8_t flag_is_time_out;

    _await delay_ticks(100);
    printf("task2 running...(%d)\n", ltx_Sys_get_tick());

    // 超时时间 1000 ticks
    while(1){
        flag_is_time_out = _await ctx_mutex_take(&mutex_test, 1000);
        if(!flag_is_time_out){
            break;
        }
        printf("task2 take mutex timeout, retry...(%d)\n", ltx_Sys_get_tick());
    }
    
    printf("task2 take mutex SUCCESS.(%d)\n", ltx_Sys_get_tick());
    _await delay_ticks(5000);
    printf("task2 give mutex.(%d)\n", ltx_Sys_get_tick());
    _await ctx_mutex_give(&mutex_test);
    printf("task2 exit.(%d)\n", ltx_Sys_get_tick());
}

_async void task_3(void){
    
    uint8_t flag_is_time_out;

    _await delay_ticks(200);
    printf("task3 running...(%d)\n", ltx_Sys_get_tick());

    // 超时时间 1000 ticks
    while(1){
        flag_is_time_out = _await ctx_mutex_take(&mutex_test, 1000);
        if(!flag_is_time_out){
            break;
        }
        printf("task3 take mutex timeout, retry...(%d)\n", ltx_Sys_get_tick());
    }
    
    printf("task3 take mutex SUCCESS.(%d)\n", ltx_Sys_get_tick());
    _await delay_ticks(5000);
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

// 另一个线程/核心
DWORD WINAPI core_1_thread(LPVOID param);

// ---------- 主线程（调度器 + 任务执行） ----------
int main(void) {
    // 初始化临界区
    InitializeCriticalSection(&__g_spin_lock);
    #ifdef ltx_cfg_USE_IDLE_SLEEP
        // 开启空闲休眠则创建唤醒休眠信号量
        g_hSemaphore = CreateSemaphore(NULL, 0, 1, NULL);
        if (g_hSemaphore == NULL) {
            printf("创建信号量失败，错误码: %ld\n", GetLastError());
            return -1;
        }
    #endif

    // 初始化内存池
    ctx_mem_pool_init();

    // 启动异步任务
    _start_async(NULL, task_1);
    co2 = _start_async(NULL, task_2);
    co3 = _start_async(NULL, task_3);

    #if (ltx_cfg_CORE_NUM > 1)
    // 创建另一个线程模拟多核
    CreateThread(NULL, 0, core_1_thread, NULL, 0, NULL);
    #endif

    // 运行调度器
    ltx_Sys_scheduler(0);
    // 后续代码不会被执行

    while(1);
    
    #ifdef ltx_cfg_USE_IDLE_SLEEP
        CloseHandle(g_hSemaphore);
    #endif

    DeleteCriticalSection(&__g_spin_lock);
    return 0;
}

// 另一个核
DWORD WINAPI core_1_thread(LPVOID param){
    (void)param;
    
    // 运行调度器
    ltx_Sys_scheduler(1);
    
    return 0;
}

#include "main.c.coro"
