#include "ctx.h"

#include "ctx_2mutex.h"

// 返回 非0 代表获取锁超时
uint8_t ctx_2mutex_take(struct ctx_2mutex_stu *mutex, TickType_t time_out){
    TickType_t last_tick = ltx_Sys_get_tick();
    while(ltx_Sys_get_tick() - last_tick < time_out){
        if(!mutex->flag_is_locked){
            return 0;
        }
    }
    return 1;
}

void ctx_2mutex_give(struct ctx_2mutex_stu *mutex){
    // _LTX_IRQ_DISABLE();
    mutex->flag_is_locked = 0;
    // _LTX_IRQ_ENABLE();

    ltx_Topic_publish(&mutex->topic);
}


extern struct coro_stu __son_placeholder__;

void _co_ctx_2mutex_take(struct coro_stu *father, struct coro_stu *co, struct ctx_2mutex_stu *mutex, TickType_t time_out){

    // 反正非 async 函数调用这个也用不了，直接吃空指针看栈回溯 debug 正好
    // if(father == NULL){
    //     return ;
    // }

    // if(time_out == 0) time_out = -1;

    // 这个函数并不是真正的 async 函数，只是对设置订阅与超时的一层封装
    // 为了避免内存管理出现问题，所以这里将它的子节点设置为占位符
    father->son = &__son_placeholder__;

    // _LTX_IRQ_DISABLE();
    // 判断锁是否已经是被持有状态
    if(!mutex->flag_is_locked){ // 未被持有，直接持有并返回
        mutex->flag_is_locked = 1;
        // _LTX_IRQ_ENABLE();
        
        ltx_Topic_publish(&(father->alarm_next_run.topic));
        return ;
    }
    // 锁被持有，则进入异步等待
    // _LTX_IRQ_ENABLE();
    // 订阅锁释放事件
    father->topic_wait_for = &mutex->topic;
    ltx_Topic_subscribe(&mutex->topic, &father->subscriber_topic);

    ltx_Alarm_add(&(father->alarm_next_run), time_out);
}

void _co_ctx_2mutex_give(struct coro_stu *father, struct coro_stu *co, struct ctx_2mutex_stu *mutex){
    
    father->son = &__son_placeholder__;
    // father->topic_wait_for = NULL;

    // _LTX_IRQ_DISABLE();
    mutex->flag_is_locked = 0;
    // _LTX_IRQ_ENABLE();

    ltx_Topic_publish(&mutex->topic);
    ltx_Topic_publish(&(father->alarm_next_run.topic));
}
