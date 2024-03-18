#include "bsp.h"

#include "log_core.h"
#include "shell_core.h"
#include "tos_cond.h"
#include "tos_core.h"
#include "tos_mutex.h"

#include <string.h>


static tos_stack_t service_task_stack[1024];
static tos_stack_t usr_task1_stack[512];
static tos_stack_t usr_task2_stack[512];
static tos_mutex_t mutex;
static tos_cond_t  cond;
static int         data = 0;


static void service_task(void* arg) {
    while (true) {
        log_proc();
        shell_proc();
        tos_task_sleep(20);
    }
}


static void usr_task1(void* arg) {
    int cnt = 0;
    tos_task_sleep(2000);
    while (true) {
#if 0
        tos_mutex_lock(&mutex);
        log_printf("%4d: task 1 get locked\n", cnt);
        tos_task_sleep(1000);
        tos_mutex_unlock(&mutex);
#else
        tos_mutex_lock(&mutex);
        log_printf("%4d: task 1 get lock\n", cnt);
        data += 1;
        log_printf("%4d: task 1 put %d data\n", cnt, data);
        tos_mutex_unlock(&mutex);
        tos_task_sleep(1000);
        log_printf("%4d: task 1 signal\n", cnt);
        tos_cond_signal(&cond);
        tos_task_sleep(1000);
#endif
        cnt++;
    }
}


static void usr_task2(void* arg) {
    int cnt = 0;
    while (true) {
#if 0
        tos_mutex_lock(&mutex);
        log_printf("%4d: task 2 get locked\n", cnt);
        tos_mutex_unlock(&mutex);
#else
        tos_mutex_lock(&mutex);
        log_printf("%4d: task 2 get lock\n", cnt);
        while (data == 0) {
            log_printf("%4d: task 2 wait cond\n", cnt);
            tos_cond_wait(&cond, &mutex);
        }

        log_printf("%4d: task 2 get %d data\n", cnt, data);
        data = 0;
        tos_mutex_unlock(&mutex);
#endif
        cnt++;
    }
}


static void bsp_init(void) {
    sysclk_init();
    sysirq_init();
    delay_init();
    uart_dbg_init(115200);

    log_printk("bsp init ok\n");
}


static void service_init(void) {
    tos_task_attr_t task;

    shell_init();
    log_init();
    tos_mutex_module_init();
    tos_cond_module_init();

    // create task
    task.task_stack_size = sizeof(service_task_stack);
    task.task_prio       = 1;
    task.task_wait_time  = 0;
    task.task_name       = "service_task";
    task.task_stack      = service_task_stack;
    tos_task_create(service_task, nullptr, &task);
}


int main() {
    bsp_init();
    tos_init();
    service_init();

    tos_task_attr_t task;

    // create task
    task.task_stack_size = sizeof(usr_task1_stack);
    task.task_prio       = 1;
    task.task_wait_time  = 0;
    task.task_name       = "usr_task1";
    task.task_stack      = usr_task1_stack;
    tos_task_create(usr_task1, nullptr, &task);

    task.task_stack_size = sizeof(usr_task2_stack);
    task.task_prio       = 1;
    task.task_wait_time  = 0;
    task.task_name       = "usr_task2";
    task.task_stack      = usr_task2_stack;
    tos_task_create(usr_task2, nullptr, &task);

    tos_mutex_init(&mutex, nullptr);
    tos_cond_init(&cond, nullptr);

    // start tos
    log_printk("tos_demo start...\n");
    tos_start();
}


int main_cmd(int argc, char* argv[]) {
    return 0;
}
