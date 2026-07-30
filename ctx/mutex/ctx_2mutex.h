/**
 * @file ctx_2mutex.h
 * @author realTiX
 * @brief ctx 配套 双任务互斥锁 组件。依赖至少为 0.5 版本的 ctx。这个互斥锁更适合只有两个任务竞争资源的情况，开销会比 mutex 更低。
 *                                  一旦超过两个任务需要竞争某一资源，那么一定不能使用 2mutex，需要改用 mutex
 * @version 0.1
 * @date 2026-07-30 (0.1，初步完成设计)
 * 
 * @copyright Copyright (c) 2026, realTiX
 * @license Apache-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __CTX_2MUTEX_H__
#define __CTX_2MUTEX_H__

#include "ltx.h"

struct ctx_2mutex_stu {
    // 这个锁是否已经被持有
    uint8_t flag_is_locked;
    
    // 事件话题
    struct ltx_Topic_stu topic;
};

// 组件结构体初始化默认参数
#define _CTX_2MUTEX_DEAFULT_CONFIG(self)    {.flag_is_locked = 0,\
                                            .topic = {.flag_is_pending = 0,\
                                            .subscriber_head = {.prev = NULL, .next = NULL},\
                                            .subscriber_tail = &(self.topic.subscriber_head), .next = NULL}}

#define _coval_ctx_2mutex_take _coval_wait_topic


// 互斥锁的占有与释放，只允许任务调用，中断内不允许调用
// 返回 非0 代表获取锁超时
uint8_t ctx_2mutex_take(struct ctx_2mutex_stu *mutex, TickType_t time_out);
// 理论上其它任务也可以调用 give，但是更建议在获取锁的任务中调用 give，这样程序流程会更清晰利于维护
void ctx_2mutex_give(struct ctx_2mutex_stu *mutex);

void _co_ctx_2mutex_take(struct coro_stu *father, struct coro_stu *co, struct ctx_2mutex_stu *mutex, TickType_t time_out);
void _co_ctx_2mutex_give(struct coro_stu *father, struct coro_stu *co, struct ctx_2mutex_stu *mutex);

#endif // __CTX_2MUTEX_H__
