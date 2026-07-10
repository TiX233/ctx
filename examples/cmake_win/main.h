#ifndef __MAIN_H__
#define __MAIN_H__

#include <windows.h>
#include <stdio.h>
#include "ltx.h"

#define DISABLE_INTERRUPTS() EnterCriticalSection(&cs)   // 关中断
#define ENABLE_INTERRUPTS()  LeaveCriticalSection(&cs)   // 开中断

extern CRITICAL_SECTION cs;

#endif // __MAIN_H__
