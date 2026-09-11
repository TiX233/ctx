#include "ctx.h"

#include "ctx_events.h"

// 等待事件组，同步阻塞版本，使用 _await 关键字调用则会使用异步非阻塞版本
uint32_t ctx_wait_events(struct ctx_events_stu *events, TickType_t time_out, uint32_t events_wait_for, ctx_events_type_e and_or){
    if(events == NULL){
        return 0x80000000;
    }
    if(time_out == 0) time_out = -1;
    
    TickType_t last_tick = ltx_Sys_get_tick();
    while(ltx_Sys_get_tick() - last_tick < time_out){
        // 判断事件组是否满足
        if(and_or == CTX_EVENTS_TYPE_OR){
            // 事件或，满足任一事件即可触发
            if(events_wait_for & events->events_now){
                return events_wait_for & events->events_now;
            }
        }else {
            // 事件与，满足所有需要触发的事件
            if(((events_wait_for) & (events->events_now)) == (events_wait_for)){
                return events_wait_for & (~events->events_now);
            }
        }
    }

    if(and_or == CTX_EVENTS_TYPE_OR){
        return 0x80000000;
    }else {
        if(((events_wait_for) & (events->events_now)) == (events_wait_for)){
            return events_wait_for & (~events->events_now);
        }
        return (events_wait_for & (~events->events_now)) | 0x80000000;
    }
}

// 等待事件组，异步非阻塞版本
void _co_ctx_wait_events(struct coro_stu *father, struct coro_stu *co,
                    struct ctx_events_stu *events, TickType_t time_out, uint32_t events_wait_for, ctx_events_type_e and_or){
    // if(father == NULL){
    //     return ;
    // }

    // 进入时就判断是否满足，满足则直接让父任务恢复执行
    if(and_or == CTX_EVENTS_TYPE_OR){
        // 事件或，满足任一事件即可触发
        if(events_wait_for & events->events_now){
            father->something = (events_wait_for & events->events_now) | 0x40000000;
            // 唤醒父协程
            ctx_coro_wake(father, 0); // 0 代表 0 tick 后唤醒
            return ;
        }else {
            father->something = events_wait_for | 0x40000000;
        }
    }else {
        // 事件与，满足所有需要触发的事件
        father->something = events_wait_for & (~events->events_now);
        if((events_wait_for & events->events_now) == events_wait_for){
            // 唤醒父协程
            ctx_coro_wake(father, 0); // 0 代表 0 tick 后唤醒
            return ;
        }
    }

    // 还不满足条件，进入异步等待
    father->topic_wait_for = &events->topic_waitting_list;
    ltx_Topic_subscribe(&events->topic_waitting_list, &father->subscriber_topic);

    ltx_Alarm_add(&father->alarm_next_run, time_out);
}


// 重置事件组状态，会清除已经等待的事件
void ctx_Events_clear(struct ctx_events_stu *events){
    _LTX_CRITICAL_INTO();
    events->events_now = 0;
    _LTX_CRITICAL_OUTO();
}

// 事件组发布事件
void ctx_Events_publish(struct ctx_events_stu *events, uint32_t events_publish){
    uint8_t flag_need_publish_topic = 0;

    _LTX_CRITICAL_INTO();
    events->events_now |= events_publish;
    // O(n)。一般来讲都是一个任务作为消费者，多个 publisher 去发布事件位，所以两边都是 O(1)。除非有多个消费者，那么就是 O(n)
    struct ltx_Topic_subscriber_stu *subscriber = events->topic_waitting_list.subscriber_head.next;
    struct ltx_Topic_subscriber_stu *subscriber_next;
    while(subscriber != NULL){
        subscriber_next = subscriber->next;
        struct coro_stu *pCo = container_of(subscriber, struct coro_stu, subscriber_topic);
        if(pCo->something & 0x40000000){ // 事件或
            if(pCo->something & events->events_now){
                pCo->something &= events->events_now;
                pCo->topic_wait_for = &events->topic;
                // ltx_Topic_subscribe(&events->topic, &pCo->subscriber_topic);
                    // 取消订阅
                    pCo->subscriber_topic.prev->next = pCo->subscriber_topic.next;
                    if(pCo->subscriber_topic.next != NULL){
                        pCo->subscriber_topic.next->prev = pCo->subscriber_topic.prev;
                        // pCo->subscriber_topic.next = NULL;
                    }

                    // 转入事件
                    pCo->subscriber_topic.next = events->topic.subscriber_head.next;
                    pCo->subscriber_topic.prev = &events->topic.subscriber_head;
                    if(events->topic.subscriber_head.next != NULL){
                        events->topic.subscriber_head.next->prev = &pCo->subscriber_topic;
                    }
                    events->topic.subscriber_head.next = &pCo->subscriber_topic;
                flag_need_publish_topic = 1;
            }
        }else { // 事件与
            if((pCo->something & events->events_now) == pCo->something){
                pCo->something = 0;
                pCo->topic_wait_for = &events->topic;
                // ltx_Topic_subscribe(&events->topic, &pCo->subscriber_topic);
                    // 取消订阅
                    pCo->subscriber_topic.prev->next = pCo->subscriber_topic.next;
                    if(pCo->subscriber_topic.next != NULL){
                        pCo->subscriber_topic.next->prev = pCo->subscriber_topic.prev;
                        // pCo->subscriber_topic.next = NULL;
                    }

                    // 转入事件
                    pCo->subscriber_topic.next = events->topic.subscriber_head.next;
                    pCo->subscriber_topic.prev = &events->topic.subscriber_head;
                    if(events->topic.subscriber_head.next != NULL){
                        events->topic.subscriber_head.next->prev = &pCo->subscriber_topic;
                    }
                    events->topic.subscriber_head.next = &pCo->subscriber_topic;
                flag_need_publish_topic = 1;
            }else {
                pCo->something &= (~events->events_now);
            }
        }

        subscriber = subscriber_next;
    }

    _LTX_CRITICAL_OUTO();
    if(flag_need_publish_topic){
        ltx_Topic_publish(&events->topic);
    }
}

// 判断事件是否超时
uint8_t ctx_Events_is_timeout(uint32_t events_bitmap){
    return (events_bitmap&0x80000000)?1:0;
}
