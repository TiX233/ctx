/**
 * @file ctx.h
 * @author realTiX
 * @brief c 无栈协程管理器，需要搭配 coro_translater.py 做源到源翻译使用，目前暂时与 ltx 调度器紧耦合
 * @version 0.6
 * @date 2026-06-28 (0.1, 初步完成设计)
 *       2026-06-29 (0.2, 补充内存分配的判断；修复 delay 用错对象的 bug)
 *       2026-06-30 (0.3, 启动调度器管理的协程可以动态创建了；增加对整条异步任务链的启停管理)
 *       2026-07-07 (0.4, 将指针变量由 uint32_t 改为 uintptr_t 兼容 64 位设备)
 *       2026-07-30 (0.5, 闹钟回调增加超时返回值置零操作，提高拓展性)
 *       2026-08-14 (0.6, 增加内存耗尽钩子函数)
 * 
 * @copyright Copyright (c) 2026, realTiX
 * @license Apache-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __CTX_H__
#define __CTX_H__

#include "ctx_config.h"

#define _async
#define _await
#define _yield()

// 已经静态创建好了 co 对象的话可以使用这个进行静态 await
#define _await_static(obj_ptr)

// 静态创建对象结构体与私有数据结构体的宏，不需要转译脚本参与，会自动创建如下内容：
// 一个 struct _coval_obj _prvdata_obj = {xxx}; 的全局变量
// 一个 struct coro_stu obj = {xxx}; 的全局变量，内部的 prv_data 指针会指向 _prvdata_obj
#define _co_static_obj(obj, func)           struct _coval_##func _prvdata_##obj;struct coro_stu obj = {.prv_data = &_prvdata_##obj}

// 非 _async 函数启动 _async 函数需要使用此宏，或者 _async 函数启动一个与自己无关的任务
// obj_ptr 传入 NULL 表示动态创建，执行完毕后会自动释放
#define _start_async(obj_ptr, func, ...)    _co_##func(NULL, obj_ptr, ##__VA_ARGS__)
// 对转译后的函数进行声明可以使用此宏
// 一般不需要手动声明，翻译脚本会创建好函数声明
#define cof_define(func, ...)               void _co_##func(struct coro_stu *father, struct coro_stu *co, ##__VA_ARGS__)


// 协程对象
// 也许应该叫协程控制块（CCB 说是
struct coro_stu {
    // uint8_t flag_is_ready;
    uint32_t step;

    void (*callback)(struct coro_stu *co);                  // 自定义回调

    struct ltx_Topic_stu *topic_wait_for;                   // 某一步骤所等待事件的话题指针
    struct ltx_Topic_subscriber_stu subscriber_topic;       // 管理协程等待事件的订阅者
    struct ltx_Alarm_stu alarm_next_run;                    // 管理协程下次运行/超时时间的闹钟
    struct ltx_Topic_subscriber_stu subscriber_alarm;       // 管理协程闹钟事件的订阅者

    struct coro_stu *father;
    struct coro_stu *son;
    // 似乎更应该叫协程帧
    void *prv_data;                                         // 私有数据，包含参数、需要保存的局部变量以及返回值
};

struct _coval_wait_topic {uint8_t _coretval_;};

// 初始化协程
void ctx_coro_init(struct coro_stu *co, void (*callback)(struct coro_stu *co));
// 直接恢复 某协程 的执行，不关心父子关系，不建议用户调用
// ticks 传入 0 则代表尽快唤醒
void ctx_coro_wake(struct coro_stu *co, TickType_t ticks);
// 暂停 某协程任务链 的执行
void ctx_coro_pause(struct coro_stu *co);
// 恢复 某协程任务链 的执行
void ctx_coro_resume(struct coro_stu *co, TickType_t ticks);

// 预设的 delay 函数实现
void delay_ticks(TickType_t ticks);                                         // *可以使用 _awiat/_await_static 调用
void _co_delay_ticks(struct coro_stu *father, struct coro_stu *co, TickType_t ticks);

// 预设的等待事件话题实现，可设置超时时间，timeout 如果为 0 则以最大计时时间进行等待
// 返回 1 代表等待事件超时
uint8_t wait_topic(struct ltx_Topic_stu *topic, TickType_t time_out);       // *可以使用 _awiat/_await_static 调用
void _co_wait_topic(struct coro_stu *father, struct coro_stu *co, struct ltx_Topic_stu *topic, TickType_t time_out);


// 内存相关函数都是弱定义，用户可替换为自己的实现。如果程序中全程没有使用到 _await 而是只使用 _await_static，那么可以不需要以下内容

// 整个系统运行前调用一次，初始化内存池
void ctx_mem_pool_init(void);
// 默认使用对象池分配，所以 size 参数此时无意义
void* ctx_mem_alloc(uint32_t size);         // 分配协程控制块 CCB，适合使用固定大小内存块
void* ctx_mem_data_alloc(uint32_t size);    // 分配协程帧，也就是局部变量，默认是固定大小内存块，其实比较适合 malloc 分配不定大小，但是没这个快

void ctx_mem_free(void *ptr);
void ctx_mem_data_free(void *ptr);

// 内存耗尽会调用此函数，用户可在这里处理异常
void ctx_mem_run_out(void);

#endif // __CTX_H__
