/**
 * @file tos_config.h
 * @brief task, stack, clock configs
 * 
 */

#ifndef _TOS_CONFIG_H_
#define _TOS_CONFIG_H_


// task config
#define TOS_MAX_PRIO_NUM_USED   8   // max prio is 31
#define TOS_MAX_TASK_NUM_USED   8   // without limit
#define TOS_IDLETASK_STACK_SIZE 512

// clock config
#define TOS_SYS_HZ              1000u
#define TOS_TICK_MS             (1000u / TOS_SYS_HZ)
#define TOS_TIME_WAIT_INFINITY  0xFFFFFFFFu
#define MCU_SYS_CLOCK           72000000u   // 72 MHz

// mutex
#define TOS_MAX_MUTEX_NUM       10u

// condition
#define TOS_MAX_COND_NUM        10u

#endif
