/**
 * 测试样例2，测试侧重点：
 * 1、是否能正确处理需要提升的变量的前缀 & 符号
 * 2、是否能正确处理需要获取异步函数返回值的情况：
 *      第一个 sensro_state_e state 接收返回值，虽然出现在 _await 前后，但是它不需要提升，因为出让前并不会使用这个变量，只有恢复后才会获取返回值；
 *      第二个 sensro_state_e state2 与第一个差不多，用于判断是否能正确处理 _await_static 关键字
 *      第三个 sensro_state_e state3 则用于测试翻译脚本是否偷懒只要是接收返回值就不提升，此处 state3 变量如果没有提升，则说明翻译脚本有问题
 *      第四个 int abc 则需要提升
 *      第五个 int abc2 作为 static 变量，不需要提升
 * 3、是否能处理没有跨越出让点但依然要提升的变量：
 *      int sensor_data3，没有跨越出让点，但是取了它的指针传给内层，出让后如果内层要读写指针，会因为它是栈上变量而出现错误。所以依然需要提升
 */

_async void func2(void){

    int sensor_data = 0;

    sensro_state_e state = _await get_sensor_data(&my_sensor, &sensor_data);
    if(state != 0){
        printf("Get sensor Failed!\n");
    }else {
        printf("Get: %d\n", sensor_data);
    }

    sensro_state_e state2 = _await_static(&sen_ctx) get_sensor_data(&my_sensor2, &sensor_data);
    if(state2 != 0){
        printf("Get sensor Failed!\n");
    }else {
        printf("Get: %d\n", sensor_data);
    }

    sensro_state_e state3 = 0;

    for(int i = 0; i < 3; i ++){
        state3 ++;
        _yield();
    }
    printf("state3: %d\n", state3);

    int sensor_data3 = 0;
    int test_data = 0;

    state3 = _await get_sensor_data(&my_sensor, &sensor_data3);
    
    if(state3 != 0){
        printf("Get sensor Failed!\n");
    }else {
        printf("Get: %d\n", sensor_data);
    }
    test_data ++;

    int abc = _await get_abc();
    printf("abc: %d\n", abc);
    _await delay_ticks(100);
    printf("abc: %d\n", abc++);

    static int abc2 = _await get_abc();
    printf("abc2: %d\n", abc2);
    _await delay_ticks(200);
    printf("abc2: %d\n", abc2++);
}