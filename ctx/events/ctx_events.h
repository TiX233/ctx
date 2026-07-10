/**
 * @file ctx_events.h
 * @author realTiX
 * @brief ctx 配套 事件组 组件，可等待最多 31 个事件并设置超时时间。支持 事件与 以及 事件或；支持多生产者多消费者
 * @version 0.1
 * @date 2026-07-10 (0.1，初步完成设计)
 * 
 * @copyright Copyright (c) 2026, realTiX
 * @license Apache-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __CTX_EVENTS_H__
#define __CTX_EVENTS_H__

#include "ltx.h"

typedef enum {
    CTX_EVENTS_TYPE_AND = 0,    // 事件与
    CTX_EVENTS_TYPE_OR = 1,     // 事件或
} ctx_events_type_e;

struct ctx_events_stu {
    uint32_t events_now;        // 已触发的事件集
    
    // 事件集话题
    struct ltx_Topic_stu topic;
};

struct _coval_ctx_wait_events {
    // 参数
    struct ctx_events_stu *events;
    // TickType_t time_out;
    uint32_t events_wait_for;
    uint8_t and_or;

    // 需要持久化的局部变量
    // (无)

    // 返回值
    uint32_t _coretval_;
};

// 等待事件组，同步阻塞版本，不建议使用。使用 _await/_await_static 关键字调用则会使用异步非阻塞版本
uint32_t ctx_wait_events(struct ctx_events_stu *events, TickType_t time_out, uint32_t events_wait_for, uint8_t and_or);
// 等待事件组，异步非阻塞版本
void _co_ctx_wait_events(struct coro_stu *father, struct coro_stu *co,
                        struct ctx_events_stu *events, TickType_t time_out, uint32_t events_wait_for, uint8_t and_or);


// 初始化事件组对象，只能调用一次
int ctx_Events_init(struct ctx_events_stu *events);
// 重置事件组状态，会清除已经触发的事件位。使用 ctx_Events_init 初始化后可以调用多次
void ctx_Events_reset(struct ctx_events_stu *events);

// 事件组发布事件
void ctx_Events_publish(struct ctx_events_stu *events, uint32_t events_publish);
// 判断事件是否超时
uint8_t ctx_Events_is_timeout(uint32_t events_bitmap);

#endif // __CTX_EVENTS_H__
