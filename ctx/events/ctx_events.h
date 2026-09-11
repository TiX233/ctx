/**
 * @file ctx_events.h
 * @author realTiX
 * @brief ctx 配套 事件组 组件，可等待最多 30 个事件并设置超时时间。支持 事件与 以及 事件或；支持多生产者多消费者
 * @version 0.3
 * @date 2026-07-10 (0.1, 初步完成设计)
 *       2026-07-11 (0.2, 增加调用时立即判断一次事件位是否满足)
 * 
 *       2026-09-11 (0.3, 适配 ltx V4 与新版 ctx 翻译规则；事件组现在不再需要创建任务以及动态分配)
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
    CTX_EVENTS_TYPE_OR = 0,     // 事件或
    CTX_EVENTS_TYPE_AND = 1,    // 事件与
} ctx_events_type_e;

struct ctx_events_stu {
    uint32_t events_now;        // 已触发的事件集
    
    // 正在等待的任务的列表
    struct ltx_Topic_stu topic_waitting_list;
    // 事件集话题
    struct ltx_Topic_stu topic;
};

// 等待事件组，同步阻塞版本，不建议使用。使用 _await/_await_static 关键字调用则会使用异步非阻塞版本。events_wait_for 最高两位请确保为 0
// 需要注意，新版为了不给事件组额外创建任务不动态分配内存，事件与 的返回值将会是还未触发的事件而不是已触发的事件！
// 例如 与等待 1011，但是只触发了 0110，那么返回值将会是 1000 0000 0000 0000 0000 0000 0000 1001（最高位为 1 代表超时）
// 或等待则保持返回已触发的事件
// 例如 或等待 1011，触发了 0110，那么返回值将会是 0010
uint32_t ctx_wait_events(struct ctx_events_stu *events, TickType_t time_out, uint32_t events_wait_for, ctx_events_type_e and_or);
// 等待事件组，异步非阻塞版本
void _co_ctx_wait_events(struct coro_stu *father, struct coro_stu *co,
                        struct ctx_events_stu *events, TickType_t time_out, uint32_t events_wait_for, ctx_events_type_e and_or);
#define __ctx_customize_retval_ctx_wait_events(co)   ((co->something & 0x40000000) ? (co->something & (~0x40000000)) : co->something); co->something = 0
#define __ctx_customize_no_free_ctx_wait_events

// 重置事件组状态，会清除已经触发的事件位
void ctx_Events_clear(struct ctx_events_stu *events);

// 事件组发布事件
void ctx_Events_publish(struct ctx_events_stu *events, uint32_t events_publish);
// 判断事件是否超时
uint8_t ctx_Events_is_timeout(uint32_t events_bitmap);

#endif // __CTX_EVENTS_H__
