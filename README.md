# C 语言无栈协程 async/await 源到源翻译器

- [C 语言无栈协程 async/await 源到源翻译器](#c-语言无栈协程-asyncawait-源到源翻译器)
  - [一、简介](#一简介)
  - [二、使用样例](#二使用样例)
  - [三、移植与部署](#三移植与部署)
  - [四、原理](#四原理)
  - [五、内置的 async 函数](#五内置的-async-函数)
  - [六、注意事项](#六注意事项)

## 一、简介

这是一个可以让你在 c 语言使用 async/await 语法糖的项目。  
你可以像编写同步任务一样编写每个异步任务并使它们并发执行，没有回调地狱，且不需要 (rt)os 参与。  

你的 async 函数会经过脚本翻译协程状态机并与调度器对接，不需要手写状态机。  
函数可复用，故而可降低层次状态机心智负担且不再容易出现状态爆炸的情况。

目前暂时紧耦合本人先前编写的 [ltx 调度器](https://github.com/TiX233/ltx)  
LTX-V3 提供高效的事件响应且低资源消耗，还支持空闲休眠与 tickless 操作。

## 二、使用样例

例如以下例程：

```c
#include "ctx.h"

#include "xxx.h"

_async void led_blink(int pin_num){
    for(int i = 3; i > 0; i --){
        gpio_set(pin_num, 1);
        _await delay_ticks(100);
        gpio_set(pin_num, 0);
        _await delay_ticks(100);
    }
}
int x;
_async void my_task1(void){

    while(1){
        // 1 号 led 闪烁三次
        _await led_blink(1);

        // 获取温度
        float temp = _await get_temp();
        printf("temp read ok\n");
        // 短暂出让
        _yield();
        printf("temp:%f\n", temp);
        x = 3;
        printf("test:%f\n", x);

        // 2 号 led 闪烁三次，协程对象是已经静态创建好的 led2_obj
        _await_static(&led2_obj) led_blink(2);
    }
}

// 这里的 xxx 就是这个文件的名字，需要手动 include
#include "xxx.c.coro"
```

你可以使用 `_async` 这个空宏标记需要被转译状态机的函数，其它 _async 函数可以使用 `_await` 来调用它  
被翻译的函数可以是任意返回值和参数  
被脚本翻译后的函数会保存在 `xxx.c.coro` 里面，所以需要手动在 `xxx.c` include 该文件在最后  
以及在 `xxx.h` 里 include `xxx.c.coro.h`  


不需要修改编译器或者安装编译器插件，即使您不使用 async，这段代码也会退化到同步阻塞，业务还可以无痛移植到有 os 的项目上

在本仓库的 examples 目录下有一些样例工程可供参考

## 三、移植与部署

**1、移植 ltx 调度器**

详细移植可以参考[调度器仓库](https://github.com/TiX233/ltx) readme 描述  
因为是裸机调度器，所以移植不会很麻烦，而且是无栈协程设计，所以你不需要关心不同架构的寄存器还有栈帧什么的

**2、移植翻译脚本**

需要电脑上有 python

需要确保编译每个 c 文件前都会调用一次 `python coro_translater.py xxx.c`  
一般编译环境都会提供编译前钩子供执行命令

另外，可以在命令最后加上 `--line` 参数，这样翻译结果会增加 `#line` 宏，所以运行翻译后的代码时，调试时会映射到您自己编写的代码上面。

**3、修改内存池大小**

在 `ctx_config.h` 中可以修改对象池和私有数据池的大小，请根据项目实际情况进行分配，如果不需要动态创建协程，那么可以不使用 `_await` 关键字而是只使用 `_await_static` 关键字，您可以使用全局变量或者手动管理内存

## 四、原理

具体原理可以观看视频或者阅读博客，这里只做简略概况。

**1、通过脚本翻译状态机**

脚本检测所有被 `_async` 关键字标注的函数，这个 `_async` 和其它关键字都是空宏，编译器不会报错

翻译后会得到一个带有 `_co_` 前缀的函数，并且参数会插入协程对象指针，所有 async 函数调用时调用的都是带前缀的翻译后的版本，也许算一种函数重载

**2、使用 include 导入翻译后的函数**

include 并不局限于 `.h` 文件，可以导入任意文件，就相当于文本插入，所以翻译脚本不用直接把结果插到源文件里，而是用户手动添加一个 `#include "xxx.c.coro"`，减小侵入性

**3、调度器**

任务的挂起与恢复需要一个调度器进行管理，这里目前使用的是我之前编写的一个裸机调度器 ltx，贯彻事件驱动，高效轻量

另外，翻译脚本是使用 ai 编写的，所以可能会出现不可靠的情况。  
如果您有更强的 ai 工具或者想手写更受掌控的版本，那么我编写了一个 [源码翻译规则(模板).c](./源码翻译规则(模板).c)，您可以参考这个翻译规则来设计更好的 python 脚本。

## 五、内置的 async 函数

目前有两个内置的 async 函数

```c
void delay_ticks(TickType_t ticks);
uint8_t wait_topic(struct ltx_Topic_stu *topic, TickType_t time_out);
```

第一个是延时函数，如果不使用 _await 关键字调用它，那么它就会退化为阻塞函数  
第二个是等待事件并设置超时时间的函数，可以让您的任务拥有事件驱动能力

## 六、拓展组件

除了内置的基本 async 函数，还有可以自行决定是否加入的拓展组件，目前有如下组件：

* events: 事件组，任务可以异步等待最多 31 个事件位；支持 事件与 以及 事件或；支持多生产者多消费者

## 七、注意事项

**1、请不要对以下函数标注 _async：**

* main 函数
* 中断服务函数
* 回调函数

因为这些函数都是被函数指针调用的，无法被翻译脚本和宏替换

`非 async 函数` 要启动一个 `async 函数`，请使用 `_start_async(obj_ptr, func, ...)` 宏，需要动态创建则 `obj_ptr` 传入 `NULL`  
`async 函数` 要启动一个与自己无关的任务也可以用这个宏

**2、内部块不允许重名局部变量！**

例如：
```c
_async void xxx(void){
    int a = 1;
    {
        int a = 9;
        printf("aa: %d\n", a);
    }
    _yield();

    printf("a: %d\n", a);
}
```

这种写法在 c 语言是合规的，但是我 **不允许** 出现这种情况  
为什么 要使用 相同的名字？平常来讲这也会导致业务代码比较混乱  
总之我不会让翻译器兼容这种情况，它们会都翻译成同一个变量，翻译结果大概如下：
```c
    _prv_data->a = 1;
    {
        _prv_data->a = 9;
        printf("aa: %d\n", _prv_data->a);
    }

    // 出让与状态流转
    // xxx

    printf("a: %d\n", _prv_data->a);
```

也就是这种代码会出 bug，需要注意
