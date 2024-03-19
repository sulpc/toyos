/**
 * @file tos_core.c
 * @brief tos core and task management module
 */

#define _IN_TOS_CORE_C_


#include "tos_core.h"
#include "tos_config.h"
#include "tos_core_.h"
#include "tos_utils.h"

#include "log_core.h"
#include <string.h>


static uint32_t    tos_get_highest_prio(uint32_t prio_mask);
static void        tos_idle_task_proc(void* args);
static tos_task_t* tos_get_free_tcb(void);
static tos_task_t* tos_task_tcb_init(tos_task_attr_t* taskAttr, tos_stack_t* taskStackPtr);


uint32_t           tos_task_prio_current;                                                // task core cpu
uint32_t           tos_task_prio_switch_to;                                              // core cpu
tos_task_t*        tos_task_current;                                                     //
tos_task_t*        tos_task_switch_to;                                                   //
tos_run_state_t    tos_state = {0};                                                      //
static tos_stack_t tos_idle_task_stack[TOS_IDLETASK_STACK_SIZE / sizeof(tos_stack_t)];   //
static tos_task_t  tos_tcb_pool[TOS_MAX_TASK_NUM_USED + 1];                              // add 1 for IDLE task


/**
 * @brief init variables used by tos
 *
 * @return true
 * @return false
 */
bool tos_init(void) {
    uint32_t        index;
    tos_task_attr_t taskAttr;

    tos_irq_diable();   // irq will be enable in tos_start

    // state init
    tos_task_current          = nullptr;
    tos_task_prio_current     = 0;
    tos_state.intr_level      = 0;
    tos_state.schedule_enable = false;
    tos_state.sys_running     = false;
    tos_state.sys_ticks       = 0;
    tos_state.task_number     = 0;

    // init ready_list, waiting_list, all_list
    tos_state.ready_task_prio_mask = 0;
    for (index = 0; index <= TOS_MAX_PRIO_NUM_USED; index++) {
        tos_queue_init(&tos_state.ready_task_list[index]);
    }
    tos_queue_init(&tos_state.waiting_task_list);
    tos_queue_init(&tos_state.all_task_list);
    tos_queue_init(&tos_state.free_tcb_list);

    // init free tcb list
    memset(tos_tcb_pool, 0, sizeof(tos_tcb_pool));
    for (index = 0; index <= TOS_MAX_TASK_NUM_USED; index++) {
        tos_queue_insert(&tos_state.free_tcb_list, &tos_tcb_pool[index].all_free_link);
    }

    // create idle task
    taskAttr.task_name       = "idle_task";
    taskAttr.task_wait_time  = 0;
    taskAttr.task_prio       = 0;   // lowest prio
    taskAttr.task_stack_size = TOS_IDLETASK_STACK_SIZE;
    taskAttr.task_stack      = tos_idle_task_stack;

    if (tos_task_create(tos_idle_task_proc, nullptr, &taskAttr) == nullptr) {
        tos_error("idle task create error!");
        return false;
    }

    return true;
}


/**
 * @brief start tos
 * @note called after tos_init
 */
void tos_start(void) {
    tos_task_prio_switch_to = tos_get_highest_prio(tos_state.ready_task_prio_mask);
    tos_task_switch_to      = get_task_by_ready_pending_link(tos_state.ready_task_list[tos_task_prio_switch_to].next);

    tos_state.sys_running     = true;
    tos_state.schedule_enable = true;

    tos_task_switch_to->task_switch_cnt++;

    tos_sys_clock_init();   // tos sys tick clock init

    // CM3 use PendSV IRQ to switch task, use a flag to tell the PendSV ISR if it need to store the context
    // for other platform, call the `switch_to` asm code derectly
    tos_task_switch_first();
}


/**
 * @brief start tos clock
 * @note much better to call it in init_task
 */
void tos_clock_start(void) {
    tos_sys_clock_init();   // tos sys tick clock init
}


/**
 * @brief called at begin of ISR
 *
 */
void tos_enter_isr(void) {
    tos_use_critical_section();

    tos_enter_critical_section();
    if (tos_state.sys_running) {
        if (tos_state.intr_level < 255) {
            tos_state.intr_level++;
        }
    }

    tos_leave_critical_section();
}


/**
 * @brief called at end of ISR
 *
 */
void tos_exit_isr(void) {
    tos_use_critical_section();

    if (tos_state.sys_running == true) {
        tos_enter_critical_section();

        if (tos_state.intr_level > 0)
            tos_state.intr_level--;

        // schedule when all intr exit
        if (tos_state.intr_level == 0 && tos_state.schedule_enable == true) {
            tos_task_prio_switch_to = tos_get_highest_prio(tos_state.ready_task_prio_mask);
            tos_task_switch_to = get_task_by_ready_pending_link(tos_state.ready_task_list[tos_task_prio_switch_to].next);

            if (tos_task_switch_to != tos_task_current) {
                tos_task_switch_to->task_switch_cnt++;
                tos_task_switch_intr();
            }
        }

        tos_leave_critical_section();
    }
}


/**
 * @brief enable schedule
 *
 */
void tos_schedule_enable(void) {
    tos_use_critical_section();

    tos_enter_critical_section();

    tos_state.schedule_enable = true;

    tos_leave_critical_section();
}


/**
 * @brief disable schedule
 *
 */
void tos_schedule_disable(void) {
    tos_use_critical_section();

    tos_enter_critical_section();

    tos_state.schedule_enable = false;

    tos_leave_critical_section();
}


/**
 * @brief create new task
 *
 * @param proc task proc
 * @param args args of task proc
 * @param attr task attr
 * @return tos_task_t* task hdl, return nullptr when failed
 */
tos_task_t* tos_task_create(tos_task_proc_t proc, void* args, tos_task_attr_t* attr) {
    tos_task_t*  task_hdl;
    tos_stack_t* task_stack_ptr;
    tos_use_critical_section();

    if (attr == nullptr || attr->task_stack == nullptr) {
        return nullptr;
    }

    tos_enter_critical_section();
    if (tos_state.intr_level > 0) {
        tos_leave_critical_section();
        return nullptr;
    }

    tos_leave_critical_section();

    tos_stack_t* stack_end = &attr->task_stack[(attr->task_stack_size / sizeof(tos_stack_t))];

    log_printk("create %15s: [%p, %p) %4d Bytes.\n", attr->task_name, attr->task_stack, stack_end,
               attr->task_stack_size);

    task_stack_ptr = tos_task_stack_frame_init(proc, args, stack_end - 1);
    task_hdl       = tos_task_tcb_init(attr, task_stack_ptr);

    if (task_hdl == nullptr) {
        tos_error("try to create task(%s, id=%d) error", attr->task_name, tos_state.task_number + 1);
        return nullptr;
    }

    if (tos_state.sys_running) {
        tos_schedule();
    }

    return task_hdl;
}


/**
 * @brief delete task
 *
 * @param task_hdl
 */
void tos_task_delete(tos_task_t* task_hdl) {
    tos_use_critical_section();

    // if (tos_state.intr_level > 0 || task_hdl == tos_task_current)
    if (tos_state.intr_level > 0) {
        return;
    }

    tos_enter_critical_section();

    // remove task from read_pending list
    tos_queue_remove(&task_hdl->ready_pending_link);
    if (tos_queue_is_empty(&tos_state.ready_task_list[task_hdl->task_prio])) {
        tos_state.ready_task_prio_mask &= ~task_hdl->task_prio_mask;
    }
    // remove task from waiting list
    tos_queue_remove(&task_hdl->waiting_link);

    // remove task from all_task list
    tos_state.task_number--;
    tos_queue_remove(&task_hdl->all_free_link);

    // insert tcb into free list
    tos_queue_insert(&tos_state.free_tcb_list, &task_hdl->all_free_link);

    task_hdl->task_state = TOS_TASK_STATE_STOP;

    tos_leave_critical_section();
}


/**
 * @brief
 *
 * @param task_hdl
 * @param prio
 * @return int32_t old prio when succeed, -1 when fail
 */
int32_t tos_set_task_prio(tos_task_t* task_hdl, uint32_t prio) {
    tos_use_critical_section();

    uint8_t prio_old = task_hdl->task_prio;

    if (prio == task_hdl->task_prio || task_hdl->task_state == TOS_TASK_STATE_STOP) {
        return -1;
    }

    // running or ready task, in read_pending list
    if (task_hdl->task_state == TOS_TASK_STATE_RUNNING || task_hdl->task_state == TOS_TASK_STATE_READY) {
        tos_enter_critical_section();

        // remove from old list
        tos_queue_remove(&task_hdl->ready_pending_link);
        if (tos_queue_is_empty(&tos_state.ready_task_list[task_hdl->task_prio])) {
            tos_state.ready_task_prio_mask &= ~task_hdl->task_prio_mask;
        }

        // add into new list
        tos_queue_insert(&tos_state.ready_task_list[prio], &task_hdl->ready_pending_link);
        tos_state.ready_task_prio_mask |= 1 << prio;

        tos_leave_critical_section();
    }

    // sleep or block task, in waiting or sem list, modify prio immediately
    task_hdl->task_prio      = prio;
    task_hdl->task_prio_mask = 1 << prio;

    if (task_hdl == tos_task_current) {
        tos_enter_critical_section();

        tos_task_prio_current = task_hdl->task_prio_mask;

        tos_leave_critical_section();
    }

    tos_schedule();

    return prio_old;
}


/**
 * @brief
 *
 * @param task_hdl
 * @return int32_t
 */
int32_t tos_get_task_prio(tos_task_t* task_hdl) {
    if (task_hdl == nullptr)
        return -1;

    return task_hdl->task_prio;
}


/**
 * @brief get task state
 *
 * @param task_hdl
 * @return tos_task_state_t
 */
tos_task_state_t tos_get_task_state(tos_task_t* task_hdl) {
    if (task_hdl == nullptr)
        return TOS_TASK_STATE_INVALID;

    return task_hdl->task_state;
}


/**
 * @brief task sleep some time
 *
 * @param nms
 */
void tos_task_sleep(uint32_t nms) {
    if (nms == 0)
        nms = 1;

    tos_use_critical_section();

    if (tos_state.intr_level > 0 || nms == 0) {
        return;
    }

    tos_enter_critical_section();

    tos_queue_remove(&tos_task_current->ready_pending_link);
    if (tos_queue_is_empty(&tos_state.ready_task_list[tos_task_current->task_prio])) {
        tos_state.ready_task_prio_mask &= ~tos_task_current->task_prio_mask;
    }
    tos_queue_init(&tos_task_current->ready_pending_link);

    tos_task_current->task_wait_time = nms / TOS_TICK_MS;
    tos_queue_insert(&tos_state.waiting_task_list, &tos_task_current->waiting_link);

    tos_leave_critical_section();

    tos_schedule();
}


/**
 * @brief task yield
 *
 */
void tos_task_yield(void) {
    tos_use_critical_section();
    // important to use critical_section
    // or maybe conflict with systick ISR

    tos_enter_critical_section();
    // remove from ready list, then add into list tail
    tos_queue_remove(&tos_task_current->ready_pending_link);
    tos_queue_insert(&tos_state.ready_task_list[tos_task_current->task_prio], &tos_task_current->ready_pending_link);
    tos_leave_critical_section();

    tos_schedule();
}


/**
 * @brief
 *
 * @return true
 * @return false
 */
bool tos_running(void) {
    return tos_state.sys_running;
}


/**
 * @brief
 *
 * @return tos_task_t*
 */
tos_task_t* tos_get_current_task(void) {
    return tos_task_current;
}


/**
 * @brief
 * @note called by cpu timer ISR
 */
void tos_time_tick(void) {
    tos_queue_node_t* list_node;
    tos_queue_node_t* list_next;
    tos_task_t*       task_hdl;
    tos_use_critical_section();

    tos_state.sys_ticks++;
    tos_enter_critical_section();

    // travel the time wait list
    for (list_node = tos_state.waiting_task_list.next; list_node != &tos_state.waiting_task_list; list_node = list_next) {
        task_hdl  = get_task_by_waiting_link(list_node);
        list_next = list_node->next;   // NOTICE: list link maybe changed in the loop

        if (task_hdl->task_wait_time != TOS_TIME_WAIT_INFINITY) {
            if (--task_hdl->task_wait_time == 0) {
                // move the task to ready list
                tos_queue_remove(&task_hdl->ready_pending_link);   // the task may block in a sem list
                tos_queue_insert(&tos_state.ready_task_list[task_hdl->task_prio], &task_hdl->ready_pending_link);
                tos_state.ready_task_prio_mask |= task_hdl->task_prio_mask;

                tos_queue_remove(&task_hdl->waiting_link);
                tos_queue_init(&task_hdl->waiting_link);   // make list_node->next==list_node
            }
        }
    }

    tos_leave_critical_section();
}


/**
 * @brief
 *
 */
void tos_schedule(void) {
    tos_use_critical_section();

    tos_enter_critical_section();

    if (tos_state.intr_level == 0 && tos_state.schedule_enable == true) {
        tos_task_prio_switch_to = tos_get_highest_prio(tos_state.ready_task_prio_mask);
        tos_task_switch_to = get_task_by_ready_pending_link(tos_state.ready_task_list[tos_task_prio_switch_to].next);

        if (tos_task_switch_to != tos_task_current) {
            tos_task_switch_to->task_switch_cnt++;
            tos_task_switch();
        }
    }

    tos_leave_critical_section();
}


/**
 * @brief init task TCB
 *
 * @param attr taskattr
 * @param task_stack_ptr ptr to task stack
 * @return tos_task_t* task hdl
 * @note upper caller ensure do not call this when free tcb is used up
 */
static tos_task_t* tos_task_tcb_init(tos_task_attr_t* attr, tos_stack_t* task_stack_ptr) {
    tos_task_t* task_hdl;
    tos_use_critical_section();

    task_hdl = tos_get_free_tcb();

    if (task_hdl == nullptr) {
        return nullptr;
    }

    // task stack: stack is high addr to low addr
    task_hdl->task_stk_ptr  = task_stack_ptr;
    task_hdl->task_stk_base = &attr->task_stack[(attr->task_stack_size / sizeof(tos_stack_t)) - 1];
    task_hdl->task_stk_size = attr->task_stack_size;
    task_hdl->task_stk_top  = attr->task_stack - 1;

    // schdule and other addr
    task_hdl->task_prio        = attr->task_prio;
    task_hdl->task_prio_mask   = 1u << attr->task_prio;
    task_hdl->task_wait_time   = attr->task_wait_time;
    task_hdl->task_name        = attr->task_name;
    task_hdl->task_id          = tos_state.task_number;
    task_hdl->task_state       = TOS_TASK_STATE_READY;
    task_hdl->task_switch_cnt  = 0;
    task_hdl->task_total_ticks = 0;

    // modify global var, enter critical section
    tos_enter_critical_section();

    // inset new task tcb to ready list
    tos_queue_insert(&tos_state.ready_task_list[task_hdl->task_prio], &task_hdl->ready_pending_link);
    tos_queue_init(&task_hdl->waiting_link);

    tos_state.ready_task_prio_mask |= task_hdl->task_prio_mask;
    tos_state.task_number++;
    tos_queue_insert(&tos_state.all_task_list, &task_hdl->all_free_link);

    tos_leave_critical_section();

    return task_hdl;
}


/**
 * get a free tcb from tos_state.free_tcb_list
 *
 *  @param   void
 *  @return  hdl of a free tcb, or nullptr
 *  @see
 *  @note
 */
static tos_task_t* tos_get_free_tcb(void) {
    tos_task_t* task_hdl;
    tos_use_critical_section();

    if (tos_queue_is_empty(&tos_state.free_tcb_list)) {
        return nullptr;
    }

    tos_enter_critical_section();

    task_hdl = get_task_by_all_free_link(tos_state.free_tcb_list.next);

    tos_queue_remove(&task_hdl->all_free_link);

    tos_leave_critical_section();

    return task_hdl;
}


/**
 * @brief get highest prio
 *
 * @param prio_mask
 * @return uint32_t
 */
static uint32_t tos_get_highest_prio(uint32_t prio_mask) {
    uint32_t prio = 31;
    while ((prio_mask & (1u << prio)) == 0) {
        prio--;
    }
    return prio;
}


/**
 * @brief idle task proc
 *
 * @param args
 */
static void tos_idle_task_proc(void* args) {
    while (true) {
    }
}
