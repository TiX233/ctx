# __ctx_customize_xxx 宏

不建议用户使用此宏，除非对本框架深度了解。

客制化操作宏，用于拉满内置组件性能，翻译脚本不需要特意识别被调用函数是否为内置组件做特殊优化，通过宏，让编译器做相关替换。  
且用于 ltx V4 多核适配

# 1、__ctx_customize_retval_被调用函数名

此宏用于将默认的返回值获取操作替换为用户自定义的获取返回值，例如以下是 wait_topic 的默认获取返回值的方式：

```c
用户接收返回值的变量 = ((struct _coval_wait_topic *)(co->son->prv_data))->_coretval_;
```

而 `ctx.h` 中定义了这个宏定义为：

```c
#define __ctx_customize_retval_wait_topic(co)   (co->topic_wait_for != NULL ? 1 : 0)
```

所以用户获取返回值将被替换成如下内容：

```c
用户接收返回值的变量 = __ctx_customize_retval_wait_topic(co);
```

也就是不会再从子对象获取数据了。因为 wait_topic 其实是对父对象内部的 topic 和 alarm 操作的一层封装，所以 son 用的是一个全局变量占位，虽然释放内存的时候会判断这个内存地址不属于内存池而不 free，但是还是有函数调用和出入临界区开销等，并且因为使用同一个全局变量占位，这样会导致多核下变量被修改。这里通过引入宏替换返回值解决了这个问题，不大改框架逻辑，也不用给 wait_topic 引入对象动态内存分配

# 2、__ctx_customize_no_free_被调用函数名


例如这里还通过配合如下宏定义让编译器不要编译 free 相关操作

```c
#define __ctx_customize_no_free_wait_topic

// _await 调用 _async 函数会生成以下内容（_await_static 则不会）
// _await 业务中的下面这部分会因为宏开关而不被编译
#ifndef __ctx_customize_no_free_wait_topic
    // free 子协程对象
    ctx_mem_data_free(co->son->prv_data);
    ctx_mem_free(co->son);
#endif
```

如前面所说，wait_topic 其实是对父对象内部的 topic 和 alarm 操作的一层封装，没有真正创建子对象，所以不需要额外 free，总之就是提升了性能
