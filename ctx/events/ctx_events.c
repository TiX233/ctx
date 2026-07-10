#include "ctx.h"

#include "ctx_events.h"

// 等待事件组，同步阻塞版本，使用 _await 关键字调用则会使用异步非阻塞版本
uint32_t ctx_wait_events(struct ctx_events_stu *events, TickType_t time_out, uint32_t events_wait_for, uint8_t and_or){
    if(events == NULL){
        return 0x80000000;
    }
    if(time_out == 0) time_out = -1;
    
    TickType_t last_tick = ltx_Sys_get_tick();
    while(ltx_Sys_get_tick() - last_tick < time_out){
            // 判断事件组是否满足
        if(and_or == CTX_EVENTS_TYPE_OR){
            // 事件或，满足任一事件即可触发
            if(events_wait_for | events->events_now){
                return events->events_now;
            }
        }else {
            // 事件与
            // 满足所有需要触发的事件
            if(((events_wait_for) & (events->events_now)) == (events_wait_for)){
                return events->events_now;
            }
        }
    }

    return events->events_now | 0x80000000;
}


// 事件组闹钟通用回调
void _ctx_events_alarm_cb(void *param){
    struct coro_stu *pCo = container_of(param, struct coro_stu, subscriber_alarm);
    struct _coval_ctx_wait_events *_prv_data= (struct _coval_ctx_wait_events *)pCo->prv_data;

    // 事件组等待超时
    // 设置超时标志位
    _prv_data->_coretval_ = 0x80000000 | _prv_data->events->events_now;
    // 取消订阅事件组
    ltx_Topic_unsubscribe(pCo->topic_wait_for, &(pCo->subscriber_topic));
    // 唤醒父协程
    // if(pCo->father == NULL){ // 没有父协程则自己 free 自己
    //     ctx_mem_data_free(pCo->prv_data);
    //     ctx_mem_free(pCo);
    // }else {
    //     // 唤醒父协程
        ctx_coro_wake(pCo->father, 0); // 0 代表 0 tick 后唤醒
    // }
}

// 事件组订阅话题通用回调
void _ctx_events_subscriber_cb(void *param){
    struct coro_stu *pCo = container_of(param, struct coro_stu, subscriber_topic);
    struct _coval_ctx_wait_events *_prv_data= (struct _coval_ctx_wait_events *)pCo->prv_data;

    // 判断事件组是否满足
    if(_prv_data->and_or == CTX_EVENTS_TYPE_OR){
        // 事件或，满足任一事件即可触发
        if(_prv_data->events_wait_for & _prv_data->events->events_now){
            goto TAG_events_trigger;
        }
    }else {
        // 事件与
        // 满足所有需要触发的事件
        if(((_prv_data->events_wait_for) & (_prv_data->events->events_now)) == (_prv_data->events_wait_for)){
            goto TAG_events_trigger;
        }
    }
    return ;

// 避免写两次以后漏改某一处
TAG_events_trigger:
    // 关闭超时闹钟
    ltx_Alarm_remove(&(pCo->alarm_next_run));
    // 取消订阅事件组
    ltx_Topic_unsubscribe(pCo->topic_wait_for, &(pCo->subscriber_topic));

    _prv_data->_coretval_ = _prv_data->events->events_now;

    // 唤醒父协程
    ctx_coro_wake(pCo->father, 0); // 0 代表 0 tick 后唤醒
    return ;
}

// 等待事件组，异步非阻塞版本
void _co_ctx_wait_events(struct coro_stu *father, struct coro_stu *co,
                        struct ctx_events_stu *events, TickType_t time_out, uint32_t events_wait_for, uint8_t and_or){
    // if(father == NULL){
    //     return ;
    // }
    
    struct _coval_ctx_wait_events *_prv_data;

    // 如果传进来的对象是空，那么代表外界期望动态创建这个协程的对象
    if(co == NULL){
        // 动态分配
        co = (struct coro_stu *)ctx_mem_alloc(sizeof(struct coro_stu));
        if(co == NULL){
            return ;
        }
        co->prv_data = (struct _coval_ctx_wait_events *)ctx_mem_data_alloc(sizeof(struct _coval_ctx_wait_events));
        if(co->prv_data == NULL){
            ctx_mem_free(co);
            return ;
        }
        // co->step = 0;
    }
    _prv_data = (struct _coval_ctx_wait_events *)co->prv_data;

    // 初始化协程对象
    co->father = father;
    if(father != NULL) father->son = co;
    // 配置状态机回调
    // ctx_coro_init(co, _cocb_ctx_wait_events);
    // 换掉 ctx_coro_init，使用事件组回调
    // co->callback = _cocb_ctx_wait_events;
    // co->topic_wait_for = NULL;
    co->subscriber_topic.prev = NULL;
    co->subscriber_topic.callback_func = _ctx_events_subscriber_cb;
    co->subscriber_topic.next = NULL;
    co->alarm_next_run.prev = NULL;
    co->alarm_next_run.next = NULL;
    // co->alarm_next_run.diff_tick = delay_ticks;
    co->alarm_next_run.topic.flag_is_pending = 0;
    co->alarm_next_run.topic.next = NULL;
    co->alarm_next_run.topic.subscriber_head.prev = NULL;
    co->alarm_next_run.topic.subscriber_head.next = &(co->subscriber_alarm);
    co->alarm_next_run.topic.subscriber_tail = &(co->subscriber_alarm);
    co->subscriber_alarm.prev = &(co->alarm_next_run.topic.subscriber_head);
    co->subscriber_alarm.next = NULL;
    co->subscriber_alarm.callback_func = _ctx_events_alarm_cb;
    co->son = NULL;
    // 初始化参数变量
    _prv_data->events = events;
    // _prv_data->time_out = time_out;
    _prv_data->events_wait_for = events_wait_for;
    _prv_data->and_or = and_or;
    
    co->topic_wait_for = &events->topic;
    ltx_Topic_subscribe(&events->topic, &co->subscriber_topic);    

    ltx_Alarm_add(&co->alarm_next_run, time_out);
}


// 初始化事件组对象，只能调用一次
int ctx_Events_init(struct ctx_events_stu *events){
    if(events == NULL){
        return -1;
    }

    events->topic.flag_is_pending = 0;
    events->topic.subscriber_head.prev = NULL;
    events->topic.subscriber_head.next = NULL;
    events->topic.subscriber_tail = &(events->topic.subscriber_head);
    events->topic.next = NULL;
    
    events->events_now = 0;
    // events->events_wait_for = 0;

    return 0;
}

// 重置事件组状态，会清除已经等待的事件。使用 ctx_Events_init 初始化后可以调用多次
void ctx_Events_reset(struct ctx_events_stu *events){
    events->events_now = 0;
    events->topic.flag_is_pending = 0;
}

// 事件组发布事件
void ctx_Events_publish(struct ctx_events_stu *events, uint32_t events_publish){
    events->events_now |= events_publish;
    ltx_Topic_publish(&events->topic);
}

// 判断事件是否超时
uint8_t ctx_Events_is_timeout(uint32_t events_bitmap){
    return (events_bitmap&0x80000000)?1:0;
}
