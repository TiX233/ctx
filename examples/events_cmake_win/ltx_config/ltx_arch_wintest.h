#ifndef __LTX_ARCH_WIN_TEST_H__
#define __LTX_ARCH_WIN_TEST_H__

#include "ltx_config.h"

// 有时候，内存不同步可能会导致一些系统配置不起作用，从而出现一些奇奇怪怪的现象
// 您可以考虑合理插入 DMB/DSB/ISB 等等指令来冲刷流水线

// 进出临界区宏
typedef CRITICAL_SECTION spin_type_t;
extern spin_type_t __g_spin_lock;
#define _LTX_CRITICAL_INTO()                EnterCriticalSection(&__g_spin_lock)
#define _LTX_CRITICAL_OUTO()                LeaveCriticalSection(&__g_spin_lock)

#define ltx_cfg_USE_SPIN_LOCK

// 空闲休眠相关配置
#ifdef ltx_cfg_USE_IDLE_SLEEP
    // 恢复调度器执行，默认为唤醒 cpu
    // 如果调度器跑在 rtos 的一个线程，那么可以设置为发送信号量
    #define _LTX_SET_SCHEDULE_FLAG()            __SEV()

    // 调度器空闲休眠回调，默认为 cpu 休眠，用户也可以当成空闲钩子使用，比如可以用来统计 cpu 使用率或者是否进入深度休眠等等
    // 如果调度器跑在 rtos 的一个线程，那么可以设置为等待信号量，超时时间为 最近一个 alalrm 的触发时间
    #define ltx_Sys_idle_in(core_id)            do{__WFE();}while(0)
#endif

// tickless 相关配置，tickless 要在时间戳调度模式更新后才能使用，现在暂时用不了
#ifdef ltx_cfg_USE_TICKLESS
    // 实际休眠时间可以比 ticks 小，因为醒来后调度器还会判断一次时间戳，然后继续传递新值要求休眠新 ticks
    // 如果实际休眠时间比 ticks 大，调度器也能正确处理需要弹出的 alarm，但是会影响任务实时性
    // 如果调度器是跑在 rtos 的一个线程内，那么可以改成等待信号量，超时时间就用 sleep_ticks，并将 _LTX_SET_SCHEDULE_FLAG(); 设置为发送信号量
    #define ltx_hook_idle_in(core_id, sleep_ticks)  do{ \
                                                        if(sleep_ticks > 2){ \
                                                            /* 关闭每毫秒 systick */
                                                            /* 这里写设置 rtc 下次闹钟的时间或者配置定时器计数值 */ \
                                                            /* 记得在相关中断里面写 _LTX_SET_SCHEDULE_FLAG(); */ \
                                                        } \
                                                        __WFE(); \
                                                        /* 开启每毫秒 systick */
                                                    }while(0)
#else
    // 只开启了空闲休眠而没开启 tickless
    #define ltx_hook_idle_in(core_id, sleep_ticks)  do{ \
                                                        __WFE(); \
                                                    }while(0)
#endif

#endif // __LTX_ARCH_WIN_TEST_H__
