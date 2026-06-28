#ifndef __CTX_CONFIG_H__
#define __CTX_CONFIG_H__

#include "ltx.h"

// 动态创建协程的对象池尺寸
// 也就是可动态创建的协程的个数
#define CO_MAX_POOL_COUNT       20
// 单个协程对象私有数据结构体的大小
// 因为是使用对象池分配，所以这个值取最大的私有数据结构体的 sizeof，单位字节
#define CO_MAX_PRVDATA_SIZE     100


#endif // __CTX_CONFIG_H__
