/**
 * 测试样例3，测试侧重点：
 * 1、对于变量生命周期关键字的处理是否到位，例如此处 flag_is_wait_topic_timeout 逻辑上是可以不用提升的，
 *     但是因为在循环体内，所以被认为生命周期覆盖了整个循环体，就自然是包括了循环体内的出让点，
 *     所以翻译脚本会把它的生命周期提升到协程帧中，此时用户可以用 _var_local 关键字确保该变量不用提升生命周期，减小内存消耗
 */

// 自动例程
_async void btn_check1(void){

    while(1){
        // 以最大时限等待按键单击事件
        uint8_t flag_is_wait_topic_timeout = _await wait_topic(&topic_btn_click, -1);
       
        if(flag_is_wait_topic_timeout){ // 如果等待按键单击事件超时
            // 快速闪烁十次 led2，动态创建
            _await led_blink(2, 100, 10);
        }else { // 在时限内发生了按键单机时间
            // 快速闪烁三次 led2，动态创建
            _await led_blink(2, 150, 3);
        }
    }
}
// 手动例程
_async void btn_check2(void){

    while(1){
        // 以最大时限等待按键单击事件
        _var_local uint8_t flag_is_wait_topic_timeout = _await wait_topic(&topic_btn_click, -1);
       
        if(flag_is_wait_topic_timeout){ // 如果等待按键单击事件超时
            // 快速闪烁十次 led2，动态创建
            _await led_blink(2, 100, 10);
        }else { // 在时限内发生了按键单机时间
            // 快速闪烁三次 led2，动态创建
            _await led_blink(2, 150, 3);
        }
    }
}

_async void fun_here(void){

    // 变量 i 并不需要提升，但是标注了强制提升关键字 _var_frame，所以需要提升到协程帧
    for(_var_frame int i = 0; i < 100; i ++){
        printf("i: %d\n", i);
    }

    //  这里 sensor_data 作为指针传递本来是要提升的，但是标注了 _var_local，所以不用提升
    _var_local int sensor_data;
    _await get_sensor(&sensor1, &sensor_data);

    // sensor_config 值传递不需要提升，不标注 _var_local，用于测试翻译器边界情况
    int sensor_config;
    _await get_xxx(&sensor2, sensor_config);
}
