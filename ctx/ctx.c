#include "ctx.h"

// 占位用，V0.8 版本开始不再需要
// struct _coval_wait_topic __wait_topic_prv_data__;
// struct coro_stu __son_placeholder__ = {.prv_data = &__wait_topic_prv_data__};

// 如果被非 async 函数调用，那么会退化为阻塞延时
// 如果有 rtos 那可以自行把这个替换为对应的非阻塞延时
ltx_weak 
void delay_ticks(TickType_t ticks){
    TickType_t last_tick = ltx_Sys_get_tick();
    while(ltx_Sys_get_tick() - last_tick < ticks);
}

// 等待话题并设置超时时间，timeout 如果为 0 则以最大计时时间进行等待
// 返回 1 代表等待事件超时
ltx_weak 
uint8_t wait_topic(struct ltx_Topic_stu *topic, TickType_t time_out){
    // 懒得写了，反正非 async 函数用不了
    // 只是空函数占位
    return 1;
}

#if (ltx_cfg_CORE_NUM > 1)
extern struct ltx_Topic_stu *ltx_sys_topic_queue_tail;
extern struct ltx_Alarm_stu ltx_sys_alarm_list;
#endif

// async 函数调用 delay_ticks 的话会被翻译脚本替换为调用这个
void _co_delay_ticks(struct coro_stu *father, struct coro_stu *co, TickType_t ticks){
    if(father == NULL){
        // 可能是被非 _async 函数调用了，退化到同步阻塞 delay
        delay_ticks(ticks);
        return ;
    }

#if (ltx_cfg_CORE_NUM > 1)
    // 多核下可能会有某个核暂停另一个核的任务
    // 为了不出现暂停后又被后续代码激活的情况，此处判断标志位进一步保证真正暂停
    _LTX_CRITICAL_INTO();
    if(father->flag_is_paused){ // 被暂停
        father->flag_is_paused = 0;

        _LTX_CRITICAL_OUTO();
        return ;
    }else {
        if(!ticks){ // 要求尽快执行
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
            return ;
        }
        // 将协程的闹钟设置在一段时间后
        // ltx_Alarm_add(&(father->alarm_next_run), ticks);
            struct ltx_Alarm_stu *pAlarm = &(ltx_sys_alarm_list);
            TickType_t tick_add = 0;

            // 为 0 则以最大值倒计时
            ticks = (ticks == 0) ? -1 : ticks;

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
                if((pAlarm->next->diff_tick + tick_add) > ticks){
                    father->alarm_next_run.diff_tick = ticks - tick_add;
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

            father->alarm_next_run.diff_tick = ticks - tick_add;
            father->alarm_next_run.prev = pAlarm;
            pAlarm->next = &father->alarm_next_run;

            _LTX_CRITICAL_OUTO();
    }
#else
    if(!ticks){ // 要求尽快执行
        ltx_Topic_publish(&(father->alarm_next_run.topic));
        return ;
    }
    // 将协程的闹钟设置在一段时间后
    ltx_Alarm_add(&(father->alarm_next_run), ticks);
#endif
}

// async 函数调用 wait_topic 的话会被翻译脚本替换为调用这个
void _co_wait_topic(struct coro_stu *father, struct coro_stu *co, struct ltx_Topic_stu *topic, TickType_t time_out){
    
    // 反正非 async 函数调用这个也用不了，直接吃空指针看栈回溯 debug 正好
    // if(father == NULL){
    //     return ;
    // }

    // if(time_out == 0) time_out = -1;

    #if (ltx_cfg_CORE_NUM > 1)
        father->topic_wait_for = topic;
        // 多核下可能会有某个核暂停另一个核的任务
        // 为了不出现暂停后又被后续代码激活的情况，此处判断标志位进一步保证真正暂停
        _LTX_CRITICAL_INTO();
        if(father->flag_is_paused){
            father->flag_is_paused = 0;

            _LTX_CRITICAL_OUTO();
            return ;
        }else {
            // ltx_Topic_subscribe(topic, &(father->subscriber_topic));
                if(father->subscriber_topic.prev != NULL){ // 已经订阅了某个话题，先取消订阅
                    father->subscriber_topic.prev->next = father->subscriber_topic.next;
                    if(father->subscriber_topic.next != NULL){
                        father->subscriber_topic.next->prev = father->subscriber_topic.prev;
                        // father->subscriber_topic.next = NULL;
                    }
                    // father->subscriber_topic.prev = NULL;
                }

                father->subscriber_topic.next = topic->subscriber_head.next;
                father->subscriber_topic.prev = &topic->subscriber_head;
                if(topic->subscriber_head.next != NULL){
                    topic->subscriber_head.next->prev = &father->subscriber_topic;
                }
                topic->subscriber_head.next = &father->subscriber_topic;

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
        }
    #else
        father->topic_wait_for = topic;
        ltx_Topic_subscribe(topic, &(father->subscriber_topic));

        ltx_Alarm_add(&(father->alarm_next_run), time_out);
    #endif
}



#if (ltx_cfg_CORE_NUM > 1)
#ifndef ltx_cfg_USE_CTX
    #error "请开启ltx对ctx的多核特殊适配钩子"
#endif
// 协程闹钟通用回调
uint8_t _co_alarm_cb(void *param){
    struct coro_stu *pCo = container_of(param, struct coro_stu, subscriber_alarm);

    // 调用回调
    return pCo->callback(pCo);
}

// 协程订阅话题通用回调
uint8_t _co_subscriber_cb(void *param){
    struct coro_stu *pCo = container_of(param, struct coro_stu, subscriber_topic);

    // 调用回调
    return pCo->callback(pCo);
}
#else
// 协程闹钟通用回调
uint8_t _co_alarm_cb(void *param){
    struct coro_stu *pCo = container_of(param, struct coro_stu, subscriber_alarm);

    if(pCo->topic_wait_for != NULL){ // 等待事件超时
        // 取消订阅该事件
        ltx_Topic_unsubscribe(&(pCo->subscriber_topic));
        pCo->topic_wait_for = NULL;
        pCo->something |= 0x80000000;
    }
    // 调用回调
    return pCo->callback(pCo);
}

// 协程订阅话题通用回调
uint8_t _co_subscriber_cb(void *param){
    struct coro_stu *pCo = container_of(param, struct coro_stu, subscriber_topic);
    // 关闭超时闹钟
    ltx_Alarm_remove(&(pCo->alarm_next_run));
    // 取消订阅该事件
    ltx_Topic_unsubscribe(&(pCo->subscriber_topic));
    pCo->topic_wait_for = NULL;

    // 调用回调
    return pCo->callback(pCo);
}
#endif


// 初始化协程
void ctx_coro_init(struct coro_stu *co, uint8_t (*callback)(struct coro_stu *co)){
    
    // if(co == NULL || callback == NULL){
    //     return -1;
    // }

    // co->step = 0;
    co->callback = callback;

    co->topic_wait_for = NULL;

    co->subscriber_topic.prev = NULL;
    co->subscriber_topic.callback_func = _co_subscriber_cb;
    co->subscriber_topic.next = NULL;

    co->alarm_next_run.prev = NULL;
    co->alarm_next_run.next = NULL;
    // co->alarm_next_run.diff_tick = delay_ticks;

    co->alarm_next_run.topic.state = 0;
    co->alarm_next_run.topic.next = NULL;
    co->alarm_next_run.topic.subscriber_head.prev = NULL;
    co->alarm_next_run.topic.subscriber_head.next = &(co->subscriber_alarm);

    co->subscriber_alarm.prev = &(co->alarm_next_run.topic.subscriber_head);
    co->subscriber_alarm.next = NULL;
    co->subscriber_alarm.callback_func = _co_alarm_cb;

    co->son = NULL;
    
	// return 0;
}

// 直接恢复 某协程 的执行，不关心父子协程关系
// ticks 传入 0 则代表尽快唤醒
void ctx_coro_wake(struct coro_stu *co, TickType_t ticks){
    if(co == NULL){
        return ;
    }
    
#if (ltx_cfg_CORE_NUM > 1)
    // 多核下可能会有某个核暂停另一个核的任务
    // 为了不出现暂停后又被后续代码激活的情况，此处判断标志位进一步保证真正暂停
    _LTX_CRITICAL_INTO();
    if(co->flag_is_paused){ // 被暂停
        co->flag_is_paused = 0;

        _LTX_CRITICAL_OUTO();
        return ;
    }else {
        if(!ticks){ // 要求尽快执行
            // ltx_Topic_publish(&(co->alarm_next_run.topic));
                // 就绪标志位置 1
                co->alarm_next_run.topic.state |= 0x01;
                // 已经存在，不推入事件队列
                if(co->alarm_next_run.topic.next != NULL || ltx_sys_topic_queue_tail == &co->alarm_next_run.topic){
                    _LTX_CRITICAL_OUTO();
                    return ;
                }

                ltx_sys_topic_queue_tail->next = &co->alarm_next_run.topic;
                ltx_sys_topic_queue_tail = &co->alarm_next_run.topic;

                _LTX_CRITICAL_OUTO();

                _LTX_SET_SCHEDULE_FLAG();
            return ;
        }
        if(co->topic_wait_for != NULL){
            // ltx_Topic_subscribe(co->topic_wait_for, &(co->subscriber_topic));
                if(co->subscriber_topic.prev != NULL){ // 已经订阅了某个话题，先取消订阅
                    co->subscriber_topic.prev->next = co->subscriber_topic.next;
                    if(co->subscriber_topic.next != NULL){
                        co->subscriber_topic.next->prev = co->subscriber_topic.prev;
                        // co->subscriber_topic.next = NULL;
                    }
                    // co->subscriber_topic.prev = NULL;
                }

                co->subscriber_topic.next = co->topic_wait_for->subscriber_head.next;
                co->subscriber_topic.prev = &co->topic_wait_for->subscriber_head;
                if(co->topic_wait_for->subscriber_head.next != NULL){
                    co->topic_wait_for->subscriber_head.next->prev = &co->subscriber_topic;
                }
                co->topic_wait_for->subscriber_head.next = &co->subscriber_topic;
        }
        // 将协程的闹钟设置在一段时间后
        // ltx_Alarm_add(&(co->alarm_next_run), ticks);
            struct ltx_Alarm_stu *pAlarm = &(ltx_sys_alarm_list);
            TickType_t tick_add = 0;

            // 为 0 则以最大值倒计时
            ticks = (ticks == 0) ? -1 : ticks;

            if(co->alarm_next_run.prev != NULL){
                co->alarm_next_run.prev->next = co->alarm_next_run.next;
                if(co->alarm_next_run.next != NULL){
                    co->alarm_next_run.next->prev = co->alarm_next_run.prev;
                    co->alarm_next_run.next->diff_tick += co->alarm_next_run.diff_tick; // 应该不会溢出
                    co->alarm_next_run.next = NULL;
                }
                co->alarm_next_run.prev = NULL;
            }

            while(pAlarm->next != NULL){
                if((pAlarm->next->diff_tick + tick_add) > ticks){
                    co->alarm_next_run.diff_tick = ticks - tick_add;
                    pAlarm->next->diff_tick -= co->alarm_next_run.diff_tick;

                    co->alarm_next_run.prev = pAlarm;
                    co->alarm_next_run.next = pAlarm->next;
                    pAlarm->next = &co->alarm_next_run;
                    co->alarm_next_run.next->prev = &co->alarm_next_run;

                    _LTX_CRITICAL_OUTO();
                    return ;
                }
                tick_add += pAlarm->next->diff_tick;

                pAlarm = pAlarm->next;
            }

            co->alarm_next_run.diff_tick = ticks - tick_add;
            co->alarm_next_run.prev = pAlarm;
            pAlarm->next = &co->alarm_next_run;

            _LTX_CRITICAL_OUTO();
    }
#else
    if(!ticks){ // 要求尽快执行
        ltx_Topic_publish(&(co->alarm_next_run.topic));
        return ;
    }

    if(co->topic_wait_for != NULL){
        ltx_Topic_subscribe(co->topic_wait_for, &(co->subscriber_topic));
    }
    ltx_Alarm_add(&(co->alarm_next_run), ticks);
#endif
}


#if (ltx_cfg_CORE_NUM == 1)
// 暂停 某协程任务 的执行，会遍历整条调用链，暂停最终的子协程
void ctx_coro_pause(struct coro_stu *co){
    while(co->son != NULL){
        co = co->son;
    }
    ltx_Alarm_remove(&(co->alarm_next_run));
    
    if(co->topic_wait_for != NULL){
        ltx_Topic_unsubscribe(&(co->subscriber_topic));
    }
}

// 恢复 某协程任务 的执行，会遍历整条调用链，唤醒最终的子协程
// ticks 传入 0 则代表尽快唤醒
void ctx_coro_resume(struct coro_stu *co, TickType_t ticks){
    if(co == NULL){
        return ;
    }
    while(co->son != NULL){
        co = co->son;
    }
    
    if(!ticks){ // 要求尽快执行
        ltx_Topic_publish(&(co->alarm_next_run.topic));
        return ;
    }

    ltx_Alarm_add(&(co->alarm_next_run), ticks);
    if(co->topic_wait_for != NULL){
        ltx_Topic_subscribe(co->topic_wait_for, &(co->subscriber_topic));
    }
}
#else
// 暂停 某协程任务 的执行，会遍历整条调用链，暂停最终的子协程
void ctx_coro_pause(struct coro_stu *co){
    _LTX_CRITICAL_INTO();
    while(co->son != NULL){
        co = co->son;
    }
    // 因为暂停和恢复发生在业务层，设计上不希望委托给 ltx，造成额外的引用开销。所以可能会出现例如：
    // 函数A 准备 _await 函数B 时，因为需要先分配 B 的内存，才能把 A 的 son 指定为 B
    // 此时 A 已经是准备暂停态，如果把 A 当做最后一个节点去暂停则没办法暂停 B，也就是这个任务最终不会被暂停。
    // 并且更重要的是，外部如果在 A 暂停期间恢复 A，并且此时 B 也在运行，那么 A 就只能获取到错误的返回值
    // 更危险的情况下，A 如果执行完毕释放了内存，此时 B 想要恢复 father 会使用到野指针
    // 所以下面这个标志位的作用就是防止无法暂停任务的边界情况，需要至少 V0.10 版本的翻译脚本
    co->flag_is_paused = 1;

    // ltx_Alarm_remove(&(co->alarm_next_run));
        // 移除可能已经就绪的 topic
        co->alarm_next_run.topic.state &= (~0x01); // 就绪标志位清零

        if(co->alarm_next_run.prev != NULL){ // 在活跃列表中
            co->alarm_next_run.prev->next = co->alarm_next_run.next;
            if(co->alarm_next_run.next != NULL){
                co->alarm_next_run.next->prev = co->alarm_next_run.prev;
                co->alarm_next_run.next->diff_tick += co->alarm_next_run.diff_tick; // 应该不会溢出
                co->alarm_next_run.next = NULL;
            }
            co->alarm_next_run.prev = NULL;
        }
    
    if(co->topic_wait_for != NULL){
        // ltx_Topic_unsubscribe(&(co->subscriber_topic));
            // 应该可以不用判断前继，暂时保留
            if(co->subscriber_topic.prev != NULL){
                co->subscriber_topic.prev->next = co->subscriber_topic.next;
                
                if(co->subscriber_topic.next != NULL){
                    co->subscriber_topic.next->prev = co->subscriber_topic.prev;
                    co->subscriber_topic.next = NULL;
                }
                co->subscriber_topic.prev = NULL;
            }
    }
    _LTX_CRITICAL_OUTO();
}

// 恢复 某协程任务 的执行，会遍历整条调用链，唤醒最终的子协程
// ticks 传入 0 则代表尽快唤醒
void ctx_coro_resume(struct coro_stu *co, TickType_t ticks){
    if(co == NULL){
        return ;
    }

    _LTX_CRITICAL_INTO();
    while(co->son != NULL){
        co = co->son;
    }
    
    if(!ticks){ // 要求尽快执行
        // ltx_Topic_publish(&(co->alarm_next_run.topic));
        // 就绪标志位置 1
        co->alarm_next_run.topic.state |= 0x01;
        // 已经存在，不推入事件队列
        if(co->alarm_next_run.topic.next != NULL || ltx_sys_topic_queue_tail == &co->alarm_next_run.topic){
            _LTX_CRITICAL_OUTO();
            return ;
        }

        ltx_sys_topic_queue_tail->next = &co->alarm_next_run.topic;
        ltx_sys_topic_queue_tail = &co->alarm_next_run.topic;

        _LTX_CRITICAL_OUTO();

        _LTX_SET_SCHEDULE_FLAG();

        return ;
    }

    // ltx_Alarm_add(&(co->alarm_next_run), ticks);
        struct ltx_Alarm_stu *pAlarm = &(ltx_sys_alarm_list);
        TickType_t tick_add = 0;

        // 为 0 则以最大值倒计时
        ticks = (ticks == 0) ? -1 : ticks;

        if(co->alarm_next_run.prev != NULL){
            co->alarm_next_run.prev->next = co->alarm_next_run.next;
            if(co->alarm_next_run.next != NULL){
                co->alarm_next_run.next->prev = co->alarm_next_run.prev;
                co->alarm_next_run.next->diff_tick += co->alarm_next_run.diff_tick; // 应该不会溢出
                co->alarm_next_run.next = NULL;
            }
            co->alarm_next_run.prev = NULL;
        }

        while(pAlarm->next != NULL){
            if((pAlarm->next->diff_tick + tick_add) > ticks){
                co->alarm_next_run.diff_tick = ticks - tick_add;
                pAlarm->next->diff_tick -= co->alarm_next_run.diff_tick;

                co->alarm_next_run.prev = pAlarm;
                co->alarm_next_run.next = pAlarm->next;
                pAlarm->next = &co->alarm_next_run;
                co->alarm_next_run.next->prev = &co->alarm_next_run;

                goto label_after_alarm_add;
            }
            tick_add += pAlarm->next->diff_tick;

            pAlarm = pAlarm->next;
        }

        co->alarm_next_run.diff_tick = ticks - tick_add;
        co->alarm_next_run.prev = pAlarm;
        pAlarm->next = &co->alarm_next_run;
        
label_after_alarm_add:
    if(co->topic_wait_for != NULL){
        // ltx_Topic_subscribe(co->topic_wait_for, &(co->subscriber_topic));
        if(co->subscriber_topic.prev != NULL){ // 已经订阅了某个话题，先取消订阅
            co->subscriber_topic.prev->next = co->subscriber_topic.next;
            if(co->subscriber_topic.next != NULL){
                co->subscriber_topic.next->prev = co->subscriber_topic.prev;
                // co->subscriber_topic.next = NULL;
            }
            // co->subscriber_topic.prev = NULL;
        }

        co->subscriber_topic.next = co->topic_wait_for->subscriber_head.next;
        co->subscriber_topic.prev = &co->topic_wait_for->subscriber_head;
        if(co->topic_wait_for->subscriber_head.next != NULL){
            co->topic_wait_for->subscriber_head.next->prev = &co->subscriber_topic;
        }
        co->topic_wait_for->subscriber_head.next = &co->subscriber_topic;
    }
    _LTX_CRITICAL_OUTO();
}
#endif


// 协程对象池
// 使用侵入式链表与 freelist，节约空间且获取空闲块与释放块只需要几个指令周期
// struct coro_stu __co_dynamic_obj_pool__[CO_MAX_POOL_SIZE];
uint8_t __co_dynamic_obj_pool__[CO_MAX_POOL_COUNT * sizeof(struct coro_stu)];
// 协程对象私有数据池
uint8_t __co_dynamic_prvdata_obj_pool__[CO_MAX_POOL_COUNT * CO_MAX_PRVDATA_SIZE];

// 内存池管理
struct _co_pool_ctrl_stu {
    void*       free_list;      // 指向第一个空闲块
    uint32_t    block_size;     // 每个块的大小（字节）
    uint32_t    block_count;    // 池中块的总数
};
struct _co_pool_ctrl_stu __co_obj_pool_ctrl = {
    .free_list = __co_dynamic_obj_pool__,
    .block_size = sizeof(struct coro_stu),
    .block_count = CO_MAX_POOL_COUNT,
};
struct _co_pool_ctrl_stu __co_prvdata_pool_ctrl = {
    .free_list = __co_dynamic_prvdata_obj_pool__,
    .block_size = CO_MAX_PRVDATA_SIZE,
    .block_count = CO_MAX_POOL_COUNT,
};

// 将一块空闲内存的前几个字节解释为 next 指针
#define CO_POOL_NEXT_PTR(block) (*(void **)(block))

// 整个系统运行前调用一次，初始化内存池
// 如果全程都没有用 _await 关键字的话，那么可以不用配置内存池空间并且初始化
ltx_weak
void ctx_mem_pool_init(void){

    // 初始化空闲链表：每个块的开头写入下一个块的地址
    uint8_t *block = __co_dynamic_obj_pool__;
    for(uint32_t i = 0; i < __co_obj_pool_ctrl.block_count - 1; i++){
        CO_POOL_NEXT_PTR(block) = block + __co_obj_pool_ctrl.block_size;
        block += __co_obj_pool_ctrl.block_size;
    }
    // 最后一个块指向 NULL
    CO_POOL_NEXT_PTR(block) = NULL;

    // 初始化空闲链表：每个块的开头写入下一个块的地址
    block = __co_dynamic_prvdata_obj_pool__;
    for(uint32_t i = 0; i < __co_prvdata_pool_ctrl.block_count - 1; i++){
        CO_POOL_NEXT_PTR(block) = block + __co_prvdata_pool_ctrl.block_size;
        block += __co_prvdata_pool_ctrl.block_size;
    }
    // 最后一个块指向 NULL
    CO_POOL_NEXT_PTR(block) = NULL;
}

// 默认使用对象池分配，所以 size 参数此时无意义
ltx_weak
void* ctx_mem_alloc(uint32_t size){

    _LTX_CRITICAL_INTO();

    void *block = __co_obj_pool_ctrl.free_list;
    if (block != NULL) {
        // 将头指针指向下一个空闲块（block 本身存着 next 指针）
        __co_obj_pool_ctrl.free_list = CO_POOL_NEXT_PTR(block);
    }

    _LTX_CRITICAL_OUTO();

    if(block == NULL){
        // 内存不足，让用户尝试从其它地方分配内存
        return ctx_mem_run_out(size);
    }
    return block;
}

// 对于私有数据结构体，私有数据结构体有可能超出单个块的尺寸，应该报错让用户提升内存池单个对象大小
ltx_weak
void* ctx_mem_data_alloc(uint32_t size){
    if(size > __co_prvdata_pool_ctrl.block_size){
        // 内存池单个对象尺寸不够导致的无法分配内存
        return ctx_mem_run_out(size);
    }

    _LTX_CRITICAL_INTO();

    void *block = __co_prvdata_pool_ctrl.free_list;
    if (block != NULL) {
        // 将头指针指向下一个空闲块（block 本身存着 next 指针）
        __co_prvdata_pool_ctrl.free_list = CO_POOL_NEXT_PTR(block);
    }

    _LTX_CRITICAL_OUTO();
    if(block == NULL){
        // 内存池空闲对象耗尽导致的内存不足
        return ctx_mem_run_out(size);
    }
    return block;
}

ltx_weak
void ctx_mem_free(void *ptr){
    if (ptr == NULL) return ;
    // 判断是否在内存池范围内
    if((uintptr_t)ptr < (uintptr_t)__co_dynamic_obj_pool__ || (uintptr_t)ptr >= (uintptr_t)(__co_dynamic_obj_pool__ + CO_MAX_POOL_COUNT * sizeof(struct coro_stu))){
        return ;
    }

    _LTX_CRITICAL_INTO();

    // 头插法，把归还的块插到 free_list 头部
    CO_POOL_NEXT_PTR(ptr) = __co_obj_pool_ctrl.free_list;
    __co_obj_pool_ctrl.free_list = ptr;

    _LTX_CRITICAL_OUTO();
}

ltx_weak
void ctx_mem_data_free(void *ptr){
    if (ptr == NULL) return ;
    // 判断是否在内存池范围内
    if((uintptr_t)ptr < (uintptr_t)__co_dynamic_prvdata_obj_pool__ || (uintptr_t)ptr >= (uintptr_t)(__co_dynamic_prvdata_obj_pool__ + CO_MAX_POOL_COUNT * CO_MAX_PRVDATA_SIZE)){
        return ;
    }

    _LTX_CRITICAL_INTO();

    // 头插法，把归还的块插到 free_list 头部
    CO_POOL_NEXT_PTR(ptr) = __co_prvdata_pool_ctrl.free_list;
    __co_prvdata_pool_ctrl.free_list = ptr;

    _LTX_CRITICAL_OUTO();
}

// 内存不足无法分配时会调用此函数，用户可在此处报错或者释放其它内存或者从其它地方分配内存
ltx_weak 
void* ctx_mem_run_out(uint32_t size){


    while(1){

    }

    return NULL;
}
