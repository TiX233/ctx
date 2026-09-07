#include "ctx.h"

#include "ctx_mutex.h"

// 返回 非0 代表获取锁超时
uint8_t ctx_mutex_take(struct ctx_mutex_stu *mutex, TickType_t time_out){
    TickType_t last_tick = ltx_Sys_get_tick();
    while(ltx_Sys_get_tick() - last_tick < time_out){
        if(!mutex->flag_is_locked){
            mutex->flag_is_locked = 1;
            return 0;
        }
    }
    return 1;
}

void ctx_mutex_give(struct ctx_mutex_stu *mutex){
    // _LTX_IRQ_DISABLE();
    mutex->flag_is_locked = 0;
    // _LTX_IRQ_ENABLE();

    #if 0
    // 弹出第一个等待锁的任务
    struct ltx_Topic_subscriber_stu *first_sub = mutex->topic_list.subscriber_head.next;
    struct coro_stu *pCo;
    if(first_sub != NULL){
        pCo = container_of(first_sub, struct coro_stu, subscriber_topic);
        // 从等待队列中弹出
        ltx_Topic_unsubscribe(first_sub);
        // 进入就绪队列
        pCo->topic_wait_for = &mutex->topic;
        ltx_Topic_subscribe(&mutex->topic, first_sub);
        
        // 唤醒正在等待锁的第一个任务
        ltx_Topic_publish(&mutex->topic);
    }
    #endif
}


// extern struct coro_stu __son_placeholder__;
#if (ltx_cfg_CORE_NUM > 1)
    extern struct ltx_Topic_stu *ltx_sys_topic_queue_tail;
    extern struct ltx_Alarm_stu ltx_sys_alarm_list;
#endif
void _co_ctx_mutex_take(struct coro_stu *father, struct coro_stu *co, struct ctx_mutex_stu *mutex, TickType_t time_out){

    // 反正非 async 函数调用这个也用不了，直接吃空指针看栈回溯 debug 正好
    // if(father == NULL){
    //     return ;
    // }

    // if(time_out == 0) time_out = -1;

    // 这个函数并不是真正的 async 函数，只是对设置订阅与超时的一层封装
    // 为了避免内存管理出现问题，所以这里将它的子节点设置为占位符
    // father->son = &__son_placeholder__;

    #if (ltx_cfg_CORE_NUM > 1)
    _LTX_CRITICAL_INTO();
    // 被另一个核暂停
    if(father->flag_is_paused){
        father->flag_is_paused = 0;
        
        // 保留状态便于恢复
        father->topic_wait_for = &mutex->topic_list;
        _LTX_CRITICAL_OUTO();

        return ;
    }
    #endif
    // 判断锁是否已经是被持有状态
    if(!mutex->flag_is_locked){ // 未被持有，直接持有并返回
        mutex->flag_is_locked = 1;
        #if (ltx_cfg_CORE_NUM > 1)
            // ltx_Topic_publish(&(father->alarm_next_run.topic));
                // 就绪标志位置 1
                father->alarm_next_run.topic.state |= 0x01;
                // 已经存在，不推入事件队列
                if(father->alarm_next_run.topic.next != NULL || ltx_sys_topic_queue_tail == &father->alarm_next_run.topic){
                    _LTX_CRITICAL_OUTO();
                    return ;
                }

                ltx_sys_topic_queue_tail->next = &father->alarm_next_run.topic;
                ltx_sys_topic_queue_tail = &father->alarm_next_run.topic;

                _LTX_CRITICAL_OUTO();

                _LTX_SET_SCHEDULE_FLAG();
        #else
            ltx_Topic_publish(&(father->alarm_next_run.topic));
        #endif

        return ;
    }
    // 锁被持有，则进入异步等待

    // 进入排队队列
    father->topic_wait_for = &mutex->topic_list;
    // ltx_Topic_subscribe(&mutex->topic_list, &father->subscriber_topic);
        if(father->subscriber_topic.prev != NULL){ // 已经订阅了某个话题，先取消订阅
            father->subscriber_topic.prev->next = father->subscriber_topic.next;
            if(father->subscriber_topic.next != NULL){
                father->subscriber_topic.next->prev = father->subscriber_topic.prev;
                // father->subscriber_topic.next = NULL;
            }
            // father->subscriber_topic.prev = NULL;
        }

        father->subscriber_topic.next = mutex->topic_list.subscriber_head.next;
        father->subscriber_topic.prev = &mutex->topic_list.subscriber_head;
        if(mutex->topic_list.subscriber_head.next != NULL){
            mutex->topic_list.subscriber_head.next->prev = &father->subscriber_topic;
        }
        mutex->topic_list.subscriber_head.next = &father->subscriber_topic;

    #if (ltx_cfg_CORE_NUM > 1)
        // ltx_Alarm_add(&(father->alarm_next_run), time_out);
            struct ltx_Alarm_stu *pAlarm = &(ltx_sys_alarm_list);
            TickType_t tick_add = 0;

            // 为 0 则以最大值倒计时
            time_out = (time_out == 0) ? -1 : time_out;

            if(father->alarm_next_run.prev != NULL){
                father->alarm_next_run.prev->next = father->alarm_next_run.next;
                if(father->alarm_next_run.next != NULL){
                    father->alarm_next_run.next->prev = father->alarm_next_run.prev;
                    father->alarm_next_run.next->diff_tick += father->alarm_next_run.diff_tick; // 应该不会溢出
                    father->alarm_next_run.next = NULL;
                }
                father->alarm_next_run.prev = NULL;
            }

            while(pAlarm->next != NULL){
                if((pAlarm->next->diff_tick + tick_add) > time_out){
                    father->alarm_next_run.diff_tick = time_out - tick_add;
                    pAlarm->next->diff_tick -= father->alarm_next_run.diff_tick;

                    father->alarm_next_run.prev = pAlarm;
                    father->alarm_next_run.next = pAlarm->next;
                    pAlarm->next = &father->alarm_next_run;
                    father->alarm_next_run.next->prev = &father->alarm_next_run;

                    _LTX_CRITICAL_OUTO();
                    return ;
                }
                tick_add += pAlarm->next->diff_tick;

                pAlarm = pAlarm->next;
            }

            father->alarm_next_run.diff_tick = time_out - tick_add;
            father->alarm_next_run.prev = pAlarm;
            pAlarm->next = &father->alarm_next_run;
        _LTX_CRITICAL_OUTO();
    #else
        ltx_Alarm_add(&(father->alarm_next_run), time_out);
    #endif
}

#if (ltx_cfg_CORE_NUM > 1)
void _co_ctx_mutex_give(struct coro_stu *father, struct coro_stu *co, struct ctx_mutex_stu *mutex){
    
    // father->son = &__son_placeholder__;
    // father->topic_wait_for = NULL;

    _LTX_CRITICAL_INTO();

    // 弹出第一个等待锁的任务
    if(mutex->topic_list.subscriber_head.next != NULL){
        struct ltx_Topic_subscriber_stu *first_sub = &mutex->topic_list.subscriber_head;

        // ltx v3 订阅者是先来先到，而 v4 的订阅者是后来先到，所以只能 O(n) 了
        // 不过应该不会同时有几千几万个任务抢互斥锁吧。。。最多三两个吧，影响应该不大，甚至可能还更快。。
        while(first_sub->next != NULL){
            first_sub = first_sub->next;
        }

        struct coro_stu *pCo = container_of(first_sub, struct coro_stu, subscriber_topic);
        // 从等待队列中弹出
        first_sub->prev->next = NULL;
        first_sub->prev = NULL;

        // 进入就绪队列
        pCo->topic_wait_for = &mutex->topic;
        
        // ltx_Alarm_remove(&pCo->alarm_next_run);
            // 移除可能已经就绪的 topic
            pCo->alarm_next_run.topic.state &= (~0x01); // 就绪标志位清零

            if(pCo->alarm_next_run.prev != NULL){ // 在活跃列表中
                pCo->alarm_next_run.prev->next = pCo->alarm_next_run.next;
                if(pCo->alarm_next_run.next != NULL){
                    pCo->alarm_next_run.next->prev = pCo->alarm_next_run.prev;
                    pCo->alarm_next_run.next->diff_tick += pCo->alarm_next_run.diff_tick; // 应该不会溢出
                    pCo->alarm_next_run.next = NULL;
                }
                pCo->alarm_next_run.prev = NULL;
            }

        // ltx_Topic_subscribe(&mutex->topic, first_sub);
            if(first_sub->prev != NULL){ // 已经订阅了某个话题，先取消订阅
                first_sub->prev->next = first_sub->next;
                if(first_sub->next != NULL){
                    first_sub->next->prev = first_sub->prev;
                    // first_sub->next = NULL;
                }
                // first_sub->prev = NULL;
            }

            first_sub->next = mutex->topic.subscriber_head.next;
            first_sub->prev = &mutex->topic.subscriber_head;
            if(mutex->topic.subscriber_head.next != NULL){
                mutex->topic.subscriber_head.next->prev = first_sub;
            }
            mutex->topic.subscriber_head.next = first_sub;

        // 唤醒正在等待锁的第一个任务
        // ltx_Topic_publish(&mutex->topic);
            // 就绪标志位置 1
            mutex->topic.state |= 0x01;
            // 不存在则推入事件队列
            if(!(mutex->topic.next != NULL || ltx_sys_topic_queue_tail == &mutex->topic)){
                ltx_sys_topic_queue_tail->next = &mutex->topic;
                ltx_sys_topic_queue_tail = &mutex->topic;

                _LTX_SET_SCHEDULE_FLAG();
            }
    }else {
        mutex->flag_is_locked = 0;
    }
    // 唤醒自己
    // 被另一个核暂停，不唤醒
    if(father->flag_is_paused){
        father->flag_is_paused = 0;
        
        _LTX_CRITICAL_OUTO();

        return ;
    }
    // ltx_Topic_publish(&(father->alarm_next_run.topic));
        // 就绪标志位置 1
        father->alarm_next_run.topic.state |= 0x01;
        // 已经存在，不推入事件队列
        if(father->alarm_next_run.topic.next != NULL || ltx_sys_topic_queue_tail == &father->alarm_next_run.topic){
            _LTX_CRITICAL_OUTO();
            return ;
        }

        ltx_sys_topic_queue_tail->next = &father->alarm_next_run.topic;
        ltx_sys_topic_queue_tail = &father->alarm_next_run.topic;

        _LTX_CRITICAL_OUTO();

        _LTX_SET_SCHEDULE_FLAG();
}
#else
void _co_ctx_mutex_give(struct coro_stu *father, struct coro_stu *co, struct ctx_mutex_stu *mutex){
    
    // father->son = &__son_placeholder__;
    // father->topic_wait_for = NULL;

    // 弹出第一个等待锁的任务
    if(mutex->topic_list.subscriber_head.next != NULL){
        struct ltx_Topic_subscriber_stu *first_sub = &mutex->topic_list.subscriber_head;

        // ltx v3 订阅者是先来先到，而 v4 的订阅者是后来先到，所以只能 O(n) 了
        // 不过应该不会同时有几千几万个任务抢互斥锁吧。。。最多三两个吧，影响应该不大，甚至可能还更快。。
        while(first_sub->next != NULL){
            first_sub = first_sub->next;
        }

        struct coro_stu *pCo = container_of(first_sub, struct coro_stu, subscriber_topic);
        // 从等待队列中弹出
        first_sub->prev->next = NULL;
        first_sub->prev = NULL;

        // 进入就绪队列
        pCo->topic_wait_for = &mutex->topic;
        
        ltx_Alarm_remove(&pCo->alarm_next_run);

        ltx_Topic_subscribe(&mutex->topic, first_sub);

        // 唤醒正在等待锁的第一个任务
        ltx_Topic_publish(&mutex->topic);
    }else {
        mutex->flag_is_locked = 0;
    }
    // 唤醒自己
    ltx_Topic_publish(&(father->alarm_next_run.topic));
}
#endif
