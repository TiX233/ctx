// 新版（V2）与旧版（V1）的区别：
//     1、业务上兼容旧版，也就是基本不需要修改业务代码
//     2、减少了一层函数调用，减少了不必要的重复传参，提高运行效率
//     3、_start_async 宏现在可以获取返回值（struct coro_stu*）了，也就是动态创建的任务可以获取到句柄，便于业务控制


// _async 函数样例：

_async 返回类型 函数名(函数参数...){
    业务代码();
    _await 业务函数(参数...);
    _yield();
    float data  = _await_static(&obj) 业务函数2(参数...);

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
 * 需要提升生命周期的局部变量: 
 *      typex xxx
 *      ...
 * 
 * 出让点行号:
 *      12, 16, ...
 * 
 * 局部变量生命周期:
 *      type5 name1: 生命周期=(起始行号, 终止行号], 是否跨越出让点=True/False
 *      ...
 */
struct _coval_函数名 {
    // 参数
    函数参数...

    // 局部变量，对于作用域没有跨域挂起点（_await、_await_static和_yield关键字）的局部变量不需要提升，static 变量不需要提升
    // 生命周期以行号起止左开右闭进行判断，如果最后出现在循环体则以循环体末尾行号作为自己的终止行号
    // 用户手动标注 _var_frame 的变量则必须提升生命周期，标注 _var_local 的变量则必须不提升生命周期
    局部变量...

    // 返回值，如果该函数是 void 返回，则使用一个 int 变量占位，名称固定为 _coretval_
    返回类型 _coretval_;
};


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

/* 函数参数与部分局部变量（作用域跨越出让点的，非static的）都需要替换为 _prv_data->局部变量... 的形式，有类型的要去除类型，例如 int a = 1; 替换为 _prv_data->a = 1; */
// 调度器回调，也就是翻译后的状态机代码
void _cocb_函数名(struct coro_stu *co){
    struct _coval_函数名 *_prv_data = (struct _coval_函数名 *)co->prv_data;

    switch(co->step){
        // xxx 由 1 起
        /* BEGIN: 根据实际情况生成跳转标签 */
        case xxx: goto _colabel_xxx;
        /* END: 根据实际情况生成跳转标签 */
    }

    /* 以下开始是用户代码，普通代码保持原样，检测到关键字则替换 */

    /* BEGIN: 检测到 _await 关键字，替换 */
    co->step = 下一步骤编号;
    _co_被调用函数名(co, NULL, 被调用函数参数...);
    return ; // 出让
    // label 必须顶格，便于区分
_colabel_下一步骤编号:
    /* 如果用户未接收返回值，那么不需要生成下面这句：*/
    用户接收返回值的变量 = ((struct _coval_被调用函数名 *)(co->son->prv_data))->_coretval_;
    // free 子协程对象
    ctx_mem_data_free(co->son->prv_data);
    ctx_mem_free(co->son);
    co->son = NULL;
    /* END: 检测到 _await 关键字，替换 */


    /* BEGIN: 检测到 _await_static 关键字，替换 */
    co->step = 下一步骤编号;
    _co_被调用函数名(co, 已静态创建好的对象指针, 被调用函数参数...);
    return ; // 出让
_colabel_下一步骤编号:
    /* 如果用户未接收返回值，那么不需要生成下面这句：*/
    用户接收返回值的变量 = ((struct _coval_被调用函数名 *)(co->son->prv_data))->_coretval_;
    co->son = NULL;
    /* END: 检测到 _await_static 关键字，替换 */

    /* BEGIN: 检测到 _yield 关键字，替换 */
    co->step = 下一步骤编号;
    ctx_coro_wake(co, 0); // 0 代表 0 tick 后唤醒，也就是告诉调度器尽快唤醒
    return ; // 出让
_colabel_下一步骤编号:
    /* END: 检测到 _yield 关键字，替换 */

    /* 如果本函数是 void 类型：*/
    /* BEGIN: 检测到用户 return，替换 */
    goto _colabel_end;
    /* END: 检测到用户 return，替换 */

    /* 如果本函数是非 void 返回类型：*/
    /* BEGIN: 检测到用户 return xxx; 替换 */
    _prv_data->_coretval_ = xxx;
    goto _colabel_end;
    /* END: 检测到用户 return xxx; 替换 */


    // 在函数最后插入收尾操作
_colabel_end:
    // co->step = 0; // 复位状态机
    if(co->father == NULL){ // 没有父协程则自己 free 自己
        ctx_mem_data_free(co->prv_data);
        ctx_mem_free(co);
    }else {
        // 唤醒父协程
        ctx_coro_wake(co->father, 0); // 0 代表 0 tick 后唤醒
    }
}

// 外界调用 _start_async 宏会转换为调用此函数，用以初始化，该函数相较旧版会返回对象指针，便于操作动态创建的对象
struct coro_stu* _co_函数名(struct coro_stu *father, struct coro_stu *co, 函数参数...){
    struct _coval_函数名 *_prv_data;

    // 如果传进来的对象是空，那么代表外界期望动态创建这个协程的对象
    if(co == NULL){
        // 动态分配
        // co 对象和 私有数据 分开分配，因为 co 的大小是固定的，更适合对象池分配
        co = (struct coro_stu *)ctx_mem_alloc(sizeof(struct coro_stu));
        if(co == NULL){ // 分配失败
            // ctx_mem_alloc 函数内部会发布错误事件，不在这里重复处理
            return NULL;
        }
        co->prv_data = (struct _coval_函数名 *)ctx_mem_data_alloc(sizeof(struct _coval_函数名));
        if(co->prv_data == NULL){ // 分配失败
            // free 对象
            ctx_mem_free(co);
            // ctx_mem_data_alloc 函数内部会发布错误事件，不在这里重复处理
            return NULL;
        }
    }
    _prv_data = (struct _coval_函数名 *)co->prv_data;

    // 初始化协程对象
    co->father = father;
    // if(father != NULL) father->son = co;
    // 配置状态机回调
    ctx_coro_init(co, _cocb_函数名);

    /* BEGIN: 根据实际情况生成不同的初始化参数变量内容 */
    _prv_data->函数参数... = 函数参数...;
    /* END: 根据实际情况生成不同的初始化参数和局部变量内容 */

    // 运行/启动该任务
    co->step = 0;
    if(father != NULL){
        father->son = co;
        // 如果是 _async 函数使用 _await/_await_static 调用，那么就地运行到出让点，不必等调度器调度，减少调度切换次数
        _cocb_函数名(co);
    }else {
        // 如果是非 _async 函数调用 _start_async 创建异步任务，那么不立即执行内容，而是等调度器调度，利于业务启停控制
        ctx_coro_wake(co, 0); // 0 代表 0 tick 后唤醒，也就是告诉调度器尽快唤醒
    }

    return co;
}
