#include "timer.h"
#include "io.h"
#include "print.h"
#include "interrupt.h"
#include "thread.h"
#include "debug.h"

#define IRQ0_FREQUENCY	   100
#define INPUT_FREQUENCY	   1193180
#define COUNTER0_VALUE	   INPUT_FREQUENCY / IRQ0_FREQUENCY
#define CONTRER0_PORT	   0x40
#define COUNTER0_NO	   0
#define COUNTER_MODE	   2
#define READ_WRITE_LATCH   3
#define PIT_CONTROL_PORT   0x43

#define mil_seconds_per_intr (1000 / IRQ0_FREQUENCY)

uint32_t ticks;    // ticks是内核自中断开启以来总共的嘀嗒数

/* 把操作的计数器counter_no、读写锁属性rwl、计数器模式counter_mode写入模式控制寄存器并赋予初始值counter_value */
static void frequency_set(uint8_t counter_port, uint8_t counter_no, uint8_t rwl, uint8_t counter_mode, uint16_t counter_value) {
/* 往控制字寄存器端口0x43中写入控制字 */
   outb(PIT_CONTROL_PORT, (uint8_t)(counter_no << 6 | rwl << 4 | counter_mode << 1));
/* 先写入counter_value的低8位 */
   outb(counter_port, (uint8_t)counter_value);
/* 再写入counter_value的高8位 */
   outb(counter_port, (uint8_t)counter_value >> 8);
}

/* 时钟的中断处理程序 */
static void intr_timer_handler(void){
    // 先通过running_thread()获取当前正在运行的线程的pcb的起始虚拟地址
    struct task_struct* cur_thread = running_thread();
    // 检查栈是否溢出, 破坏了线程信息
    ASSERT(cur_thread->stack_magic == 0x19980924);

    cur_thread->elapsed_ticks++;    // 记录此线程占用的cpu时间
    ticks++;

    if(cur_thread->ticks == 0) {    // 若进程时间片用完, 就开始调度新的进程上cpu
        schedule();
    }else{
        cur_thread->ticks--;
    }
}

/* 以tick为单位的sleep,任何时间形式的sleep会转换此ticks形式 */
static void ticks_to_sleep(uint32_t sleep_ticks) {
    uint32_t start_tick = ticks;

    /* 若间隔的ticks数不够便让出cpu */
    while (ticks - start_tick < sleep_ticks) {
        thread_yield();
    }
}

/* 以毫秒为单位的sleep   1秒= 1000毫秒 */
void mtime_sleep(uint32_t m_seconds) {
    uint32_t sleep_ticks = DIV_ROUND_UP(m_seconds, mil_seconds_per_intr);
    ASSERT(sleep_ticks > 0);
    ticks_to_sleep(sleep_ticks);
}

/* 初始化PIT8253 */
void timer_init() {
    put_str("timer_init start\n");
    /* 设置8253的定时周期,也就是发中断的周期 */
    frequency_set(CONTRER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER_MODE, COUNTER0_VALUE);
    // 注册时钟中断处理程序
    register_handler(0x20, intr_timer_handler);
    put_str("timer_init done\n");
}