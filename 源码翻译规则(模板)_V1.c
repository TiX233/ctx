// _async 函数样例：

_async 返回类型 函数名(函数参数...){
    业务代码();
    _await 业务函数(参数...);
    _yield();
    float data;
    data  = _await 业务函数2(参数...);

    return xxx;
}

// 翻译后模板
/**
 * 识别到 _async 关键字
 * 函数名称: xxx
 * 返回类型: xxx
 * 起始行号: 
 * 终止行号:
 * 
 * 参数: type1 xxx1
 *       type2 xxx2
 *       ....
 * 
 * 局部变量: ...
 */
struct _coval_函数名 {
    // 参数
    函数参数...

    // 局部变量，对于作用域没有跨域挂起点（_await、_await_static和_yield关键字）的局部变量不需要提升
    局部变量...

    // 返回值，如果该函数是 void 返回，则使用一个 int 变量占位，名称固定为 _coretval_
    返回类型 _coretval_;
};

// 调度器回调
void _cocb_函数名(struct coro_stu *co);

/* 函数内部所有局部变量都需要替换为 _prv_data->局部变量... 的形式（参数也要），有类型的要去除类型，例如 int a = 1; 替换为 _prv_data->a = 1; */
/* 
内部块不允许重名局部变量！例如：
void xxx(void){
    int a = 1;
    {
        int a = 9;
        printf("aa: %d\n", a);
    }

    printf("a: %d\n", a);
}
这种写法在 c 语言是合规的，但是我   **不允许**   出现这种情况
你 为什么 要使用 相同的名字？平常来讲这也会导致业务代码比较混乱
总之我的翻译器不会帮你擦屁股，它们会都翻译成同一个变量，翻译结果大概如下：
    _prv_data->a = 1;
    {
        _prv_data->a = 9;
        printf("aa: %d\n", _prv_data->a);
    }
    printf("a: %d\n", _prv_data->a);

也就是你的代码会出 bug，这是你应得的
*/
void _co_函数名(struct coro_stu *father, struct coro_stu *co, 函数参数...){
    struct _coval_函数名 *_prv_data;

    // 如果传进来的对象是空，那么代表外界期望动态创建这个协程的对象
    if(co == NULL){
        // 动态分配
        // co 对象和 私有数据 分开分配，因为 co 的大小是固定的，更适合对象池分配
        co = (struct coro_stu *)ctx_mem_alloc(sizeof(struct coro_stu));
        if(co == NULL){ // 分配失败
            // ctx_mem_alloc 函数内部会发布错误事件，不在这里重复处理，并且这里没有办法返回错误
            return ;
        }
        co->prv_data = (struct _coval_函数名 *)ctx_mem_data_alloc(sizeof(struct _coval_函数名));
        if(co->prv_data == NULL){ // 分配失败
            // free 对象
            ctx_mem_free(co);
            // ctx_mem_data_alloc 函数内部会发布错误事件，不在这里重复处理，并且这里没有办法返回错误
            return ;
        }
        co->step = 0;
    }
    _prv_data = (struct _coval_函数名 *)co->prv_data;

    switch(co->step){
        case 0: goto _colable_函数名_0;
        /* BEGIN: 根据实际情况生成跳转标签 */
        case xxx: goto _colable_函数名_xxx;
        /* END: 根据实际情况生成跳转标签 */
    }

    // 步骤 0 用于初始化
    // label 必须顶格，便于区分
_colable_函数名_0:
    // 初始化协程对象
    co->father = father;
    if(father != NULL) father->son = co;
    // 配置状态机回调
    ctx_coro_init(co, _cocb_函数名);

    /* BEGIN: 根据实际情况生成不同的初始化参数变量内容 */
    _prv_data->函数参数... = 函数参数...;
    /* END: 根据实际情况生成不同的初始化参数和局部变量内容 */

    /* 以下开始是用户代码 */
    

    /* BEGIN: 检测到 _await 关键字，替换 */
    _co_被调用函数名(co, NULL, 被调用函数参数...);
    co->step = 下一步骤编号;
    return ; // 出让
_colable_函数名_下一步骤编号:
    /* 如果用户未接收返回值，那么不需要生成下面这句：*/
    用户接收返回值的变量 = ((struct _coval_被调用函数名 *)(co->son->prv_data))->_coretval_;
    // free 子协程对象
    ctx_mem_data_free(co->son->prv_data);
    ctx_mem_free(co->son);
    co->son = NULL;
    /* END: 检测到 _await 关键字，替换 */


    /* BEGIN: 检测到 _await_static 关键字，替换 */
    _co_被调用函数名(co, 已静态创建好的对象指针, 被调用函数参数...);
    co->step = 下一步骤编号;
    return ; // 出让
_colable_函数名_下一步骤编号:
    /* 如果用户未接收返回值，那么不需要生成下面这句：*/
    用户接收返回值的变量 = ((struct _coval_被调用函数名 *)(co->son->prv_data))->_coretval_;
    co->son = NULL;
    /* END: 检测到 _await_static 关键字，替换 */

    /* BEGIN: 检测到 _yield 关键字，替换 */
    ctx_coro_wake(co, 0); // 0 代表 0 tick 后唤醒，也就是告诉调度器尽快唤醒
    co->step = 下一步骤编号;
    return ; // 出让
_colable_函数名_下一步骤编号:
    /* END: 检测到 _yield 关键字，替换 */

    /* 如果本函数是 void 类型：*/
    /* BEGIN: 检测到用户 return，替换 */
    goto _colable_函数名_end;
    /* END: 检测到用户 return，替换 */

    /* 如果本函数是非 void 返回类型：*/
    /* BEGIN: 检测到用户 return xxx; 替换 */
    _prv_data->_coretval_ = xxx;
    goto _colable_函数名_end;
    /* END: 检测到用户 return xxx; 替换 */


    // 在函数最后插入收尾操作
_colable_函数名_end:
    co->step = 0; // 复位状态机
    if(father == NULL){ // 没有父协程则自己 free 自己
        ctx_mem_data_free(co->prv_data);
        ctx_mem_free(co);
    }else {
        // 唤醒父协程
        ctx_coro_wake(father, 0); // 0 代表 0 tick 后唤醒
    }
}

// 封装给调度器用的通用回调
void _cocb_函数名(struct coro_stu *co){
    _co_函数名(co->father, co, ((struct _coval_函数名 *)co->prv_data)->函数参数...);
}
