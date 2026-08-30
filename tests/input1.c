/**
 * 测试样例1，测试侧重点：
 * 1、是否能识别非标准变量类型（param_type1, var_type, mmm）
 * 2、是否能识别超过一个单词的类型（struct random_stu *, unsigned char）
 * 3、是否会把 * 黏附在变量名上
 * 4、是否能识别循环体内的变量作用域，例如这里必须提升变量 a，如果翻译脚本认为没有局部变量需要提升，那么则无法通过测试
 * 5、是否会在识别循环体变量时偷懒，例如此处 b 变量无需提升，如果翻译脚本提升了，那么说明它作用域识别偷懒为只要循环体就提升
 */

_async struct random_stu *func1(param_type1 x, unsigned char y, mmm *z){
    var_type a = 10;
    printf("z:%d\n", *z);

    while(1){
        printf("%d\n", a++);
        _yield();
    }
    
    int b = 9;
    for(int i = 0; i < 100; i ++){
        b ++;
    }
    printf("b:%d\n", b);
}