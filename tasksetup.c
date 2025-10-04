#include "tasksetup.h"
#include <sys/timerfd.h>
#include <sched.h>

int timer_fd = -1;

int setup_task(int zykluszeit_ms) {
    /************** Setup Priority **************/
    struct sched_param sp;
    sp.sched_priority = 20; // Wertebereich: 1–99 (höher = wichtiger)
    if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0)
        return -1;

    /************** Setup Timer **************/
    timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timer_fd < 0)
        return -2;
    struct itimerspec period = {
        .it_interval = {0, zykluszeit_ms * 1000000},
        .it_value = {0, zykluszeit_ms * 1000000}
    };
    if (timerfd_settime(timer_fd, 0, &period, NULL))
        return -3;
    return 0;
}
