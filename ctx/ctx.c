#include "ctx.h"

// 占位用
struct _coval_wait_topic __wait_topic_prv_data__;
struct coro_stu __son_placeholder__ = {.prv_data = &__wait_topic_prv_data__};

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


// async 函数调用 delay_ticks 的话会被翻译脚本替换为调用这个
void _co_delay_ticks(struct coro_stu *father, struct coro_stu *co, TickType_t ticks){
    if(father == NULL){
        // 可能是被非 _async 函数调用了，退化到同步阻塞 delay
        delay_ticks(ticks);
        return ;
    }
    // 这个函数并不是真正的 async 函数，只是对设置闹钟的一层封装
    // 为了避免内存管理出现问题，所以这里将它的子节点设置为占位符
    father->son = &__son_placeholder__;

    if(!ticks){ // 要求尽快执行
        ltx_Topic_publish(&(father->alarm_next_run.topic));
        return ;
    }
    // 将协程的闹钟设置在一段时间后
    ltx_Alarm_add(&(father->alarm_next_run), ticks);
}

// async 函数调用 wait_topic 的话会被翻译脚本替换为调用这个
void _co_wait_topic(struct coro_stu *father, struct coro_stu *co, struct ltx_Topic_stu *topic, TickType_t time_out){
    
    // 反正非 async 函数调用这个也用不了，直接吃空指针看栈回溯 debug 正好
    // if(father == NULL){
    //     return ;
    // }

    // if(time_out == 0) time_out = -1;

    father->topic_wait_for = topic;
    ltx_Topic_subscribe(topic, &(father->subscriber_topic));

    ltx_Alarm_add(&(father->alarm_next_run), time_out);
    
    // 这个函数并不是真正的 async 函数，只是对设置订阅与超时的一层封装
    // 为了避免内存管理出现问题，所以这里将它的子节点设置为占位符
    father->son = &__son_placeholder__;
}



// 协程闹钟通用回调
void _co_alarm_cb(void *param){
    struct coro_stu *pCo = container_of(param, struct coro_stu, subscriber_alarm);

    if(pCo->topic_wait_for != NULL){ // 等待事件超时
        // 取消订阅该事件
        ltx_Topic_unsubscribe(pCo->topic_wait_for, &(pCo->subscriber_topic));
        pCo->topic_wait_for = NULL;
        __wait_topic_prv_data__._coretval_ = 1;
    }else {
        __wait_topic_prv_data__._coretval_ = 0;
    }
    // 调用回调
    pCo->callback(pCo);
}

// 协程订阅话题通用回调
void _co_subscriber_cb(void *param){
    struct coro_stu *pCo = container_of(param, struct coro_stu, subscriber_topic);
    // 关闭超时闹钟
    ltx_Alarm_remove(&(pCo->alarm_next_run));
    // 取消订阅该事件
    ltx_Topic_unsubscribe(pCo->topic_wait_for, &(pCo->subscriber_topic));
    pCo->topic_wait_for = NULL;
    __wait_topic_prv_data__._coretval_ = 0;

    // 调用回调
    pCo->callback(pCo);
}



// 初始化协程
void ctx_coro_init(struct coro_stu *co, void (*callback)(struct coro_stu *co)){
    
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

    co->alarm_next_run.topic.flag_is_pending = 0;
    co->alarm_next_run.topic.next = NULL;
    co->alarm_next_run.topic.subscriber_head.prev = NULL;
    co->alarm_next_run.topic.subscriber_head.next = &(co->subscriber_alarm);
    co->alarm_next_run.topic.subscriber_tail = &(co->subscriber_alarm);

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
    
    if(!ticks){ // 要求尽快执行
        ltx_Topic_publish(&(co->alarm_next_run.topic));
        return ;
    }

    ltx_Alarm_add(&(co->alarm_next_run), ticks);
    if(co->topic_wait_for != NULL){
        ltx_Topic_subscribe(co->topic_wait_for, &(co->subscriber_topic));
    }
}

// 暂停 某协程任务 的执行，会遍历整条调用链，暂停最终的子协程
void ctx_coro_pause(struct coro_stu *co){
    while(co->son != NULL && co->son != &__son_placeholder__){
        co = co->son;
    }
    ltx_Alarm_remove(&(co->alarm_next_run));
    
    if(co->topic_wait_for != NULL){
        ltx_Topic_unsubscribe(co->topic_wait_for, &(co->subscriber_topic));
    }
}

// 恢复 某协程任务 的执行，会遍历整条调用链，唤醒最终的子协程
// ticks 传入 0 则代表尽快唤醒
void ctx_coro_resume(struct coro_stu *co, TickType_t ticks){
    if(co == NULL){
        return ;
    }
    while(co->son != NULL && co->son != &__son_placeholder__){
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

    _LTX_IRQ_DISABLE();

    void *block = __co_obj_pool_ctrl.free_list;
    if (block != NULL) {
        // 将头指针指向下一个空闲块（block 本身存着 next 指针）
        __co_obj_pool_ctrl.free_list = CO_POOL_NEXT_PTR(block);
    }

    _LTX_IRQ_ENABLE();

    if(block == NULL){
        // 内存不足
        ctx_mem_run_out();
    }
    return block;
}

// 对于私有数据结构体，私有数据结构体有可能超出单个块的尺寸，应该报错让用户提升内存池单个对象大小
ltx_weak
void* ctx_mem_data_alloc(uint32_t size){
    if(size > __co_prvdata_pool_ctrl.block_size){
        // 内存池单个对象尺寸不够导致的无法分配内存
        ctx_mem_run_out();
        return NULL;
    }

    _LTX_IRQ_DISABLE();

    void *block = __co_prvdata_pool_ctrl.free_list;
    if (block != NULL) {
        // 将头指针指向下一个空闲块（block 本身存着 next 指针）
        __co_prvdata_pool_ctrl.free_list = CO_POOL_NEXT_PTR(block);
    }

    _LTX_IRQ_ENABLE();
    if(block == NULL){
        // 内存池空闲对象耗尽导致的内存不足
        ctx_mem_run_out();
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

    _LTX_IRQ_DISABLE();

    // 头插法，把归还的块插到 free_list 头部
    CO_POOL_NEXT_PTR(ptr) = __co_obj_pool_ctrl.free_list;
    __co_obj_pool_ctrl.free_list = ptr;

    _LTX_IRQ_ENABLE();
}

ltx_weak
void ctx_mem_data_free(void *ptr){
    if (ptr == NULL) return ;
    // 判断是否在内存池范围内
    if((uintptr_t)ptr < (uintptr_t)__co_dynamic_prvdata_obj_pool__ || (uintptr_t)ptr >= (uintptr_t)(__co_dynamic_prvdata_obj_pool__ + CO_MAX_POOL_COUNT * CO_MAX_PRVDATA_SIZE)){
        return ;
    }

    _LTX_IRQ_DISABLE();

    // 头插法，把归还的块插到 free_list 头部
    CO_POOL_NEXT_PTR(ptr) = __co_prvdata_pool_ctrl.free_list;
    __co_prvdata_pool_ctrl.free_list = ptr;

    _LTX_IRQ_ENABLE();
}

// 内存不足无法分配时会调用此函数
ltx_weak 
void ctx_mem_run_out(void){
    while(1){

    }
}
