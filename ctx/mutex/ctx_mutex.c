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
    #endif
    // 判断锁是否已经是被持有状态
    if(!mutex->flag_is_locked){ // 未被持有，直接持有并返回
        mutex->flag_is_locked = 1;
        #if (ltx_cfg_CORE_NUM > 1)
        _LTX_CRITICAL_OUTO();
        #endif
        
        ltx_Topic_publish(&(father->alarm_next_run.topic));
        return ;
    }
    // 锁被持有，则进入异步等待

    // 订阅锁释放事件
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
    _LTX_CRITICAL_OUTO();
    #endif

    ltx_Alarm_add(&(father->alarm_next_run), time_out);
}

void _co_ctx_mutex_give(struct coro_stu *father, struct coro_stu *co, struct ctx_mutex_stu *mutex){
    
    // father->son = &__son_placeholder__;
    // father->topic_wait_for = NULL;

    #if (ltx_cfg_CORE_NUM > 1)
    _LTX_CRITICAL_INTO();
    #endif

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
        #if (ltx_cfg_CORE_NUM > 1)
        _LTX_CRITICAL_OUTO();
        #endif
        
        ltx_Topic_subscribe(&mutex->topic, first_sub);

        // 唤醒正在等待锁的第一个任务
        ltx_Topic_publish(&mutex->topic);
    }else {
        mutex->flag_is_locked = 0;
        #if (ltx_cfg_CORE_NUM > 1)
        _LTX_CRITICAL_OUTO();
        #endif
    }
    // 唤醒自己
    ltx_Topic_publish(&(father->alarm_next_run.topic));
}
