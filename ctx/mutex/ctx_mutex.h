/**
 * @file ctx_mutex.h
 * @author realTiX
 * @brief ctx 配套 互斥锁 组件。依赖至少为 0.8 版本的 ctx。目前暂时不支持递归。
 * @version 0.6
 * @date 2026-07-30 (0.1，初步完成设计，仅支持两个任务竞争资源)
 *       2026-07-31 (0.2，支持多任务竞争资源)
 * 
 *       2026-09-05 (0.3，适配 ltx v4)
 *       2026-09-06 (0.4，修复 give 后互斥锁未上锁的 bug)
 *       2026-09-07 (0.5，适配多核下任务暂停/恢复)
 *       2026-09-08 (0.6, 配合 ltx 增加多核下启动调度信号置于临界区外以及 tickless 适配)
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

// #define _coval_ctx_mutex_take _coval_wait_topic


// 互斥锁的占有与释放，只允许任务调用，中断内不允许调用
// 返回 非0 代表获取锁超时
uint8_t ctx_mutex_take(struct ctx_mutex_stu *mutex, TickType_t time_out);
// 理论上其它任务也可以调用 give，但是更建议在获取锁的任务中调用 give，这样程序流程会更清晰利于维护
void ctx_mutex_give(struct ctx_mutex_stu *mutex);

// #define __ctx_customize_retval_ctx_mutex_take(co)   (co->topic_wait_for != NULL ? 1 : 0); co->topic_wait_for = NULL
#define __ctx_customize_retval_ctx_mutex_take(co)   (co->something ? 1 : 0); co->something = 0
#define __ctx_customize_no_free_ctx_mutex_take
#define __ctx_customize_no_free_ctx_mutex_give
void _co_ctx_mutex_take(struct coro_stu *father, struct coro_stu *co, struct ctx_mutex_stu *mutex, TickType_t time_out);
void _co_ctx_mutex_give(struct coro_stu *father, struct coro_stu *co, struct ctx_mutex_stu *mutex);

#endif // __CTX_MUTEX_H__
