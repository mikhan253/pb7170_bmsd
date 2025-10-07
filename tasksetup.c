#include "tasksetup.h"
#include <sys/timerfd.h>
#include <sched.h>

int g_timerFd = -1;

int tas_Init(int cycletimeMs) {
    /************** Setup Priority **************/
    struct sched_param sp;
    sp.sched_priority = 20; // Wertebereich: 1–99 (höher = wichtiger)
    if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0)
        return -1;

    /************** Setup Timer **************/
    g_timerFd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (g_timerFd < 0)
        return -2;
    struct itimerspec period = {
        .it_interval = {0, cycletimeMs * 1000000},
        .it_value = {0, cycletimeMs * 1000000}
    };
    if (timerfd_settime(g_timerFd, 0, &period, NULL))
        return -3;
    return 0;
}
