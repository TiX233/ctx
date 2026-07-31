/**
 * @file ctx_mutex.h
 * @author realTiX
 * @brief ctx 配套 互斥锁 组件。依赖至少为 0.5 版本的 ctx。
 * @version 0.2
 * @date 2026-07-30 (0.1，初步完成设计，仅支持两个任务竞争资源)
 *       2026-07-31 (0.2，支持多任务竞争资源)
 * 
 * @copyright Copyright (c) 2026, realTiX
 * @license Apache-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __CTX_MUTEX_H__
#define __CTX_MUTEX_H__

#include "ltx.h"

struct ctx_mutex_stu {
    // 这个锁是否已经被持有
    uint8_t flag_is_locked;
    
    // 等待任务列表，按照时间顺序推入
    struct ltx_Topic_stu topic_list;
    // 事件话题
    struct ltx_Topic_stu topic;
};

// 组件结构体初始化默认参数
#define _CTX_MUTEX_DEAFULT_CONFIG(self)     {.flag_is_locked = 0,\
                                            .topic_list = {.flag_is_pending = 0,\
                                            .subscriber_head = {.prev = NULL, .next = NULL},\
                                            .subscriber_tail = &(self.topic_list.subscriber_head), .next = NULL},\
                                            .topic = {.flag_is_pending = 0,\
                                            .subscriber_head = {.prev = NULL, .next = NULL},\
                                            .subscriber_tail = &(self.topic.subscriber_head), .next = NULL}}

#define _coval_ctx_mutex_take _coval_wait_topic


// 互斥锁的占有与释放，只允许任务调用，中断内不允许调用
// 返回 非0 代表获取锁超时
uint8_t ctx_mutex_take(struct ctx_mutex_stu *mutex, TickType_t time_out);
// 理论上其它任务也可以调用 give，但是更建议在获取锁的任务中调用 give，这样程序流程会更清晰利于维护
void ctx_mutex_give(struct ctx_mutex_stu *mutex);

void _co_ctx_mutex_take(struct coro_stu *father, struct coro_stu *co, struct ctx_mutex_stu *mutex, TickType_t time_out);
void _co_ctx_mutex_give(struct coro_stu *father, struct coro_stu *co, struct ctx_mutex_stu *mutex);

#endif // __CTX_MUTEX_H__
