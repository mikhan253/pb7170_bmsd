#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/timerfd.h>
#include <stdint.h>
#include <errno.h>

void cyclic_task() {
    static uint64_t last_ms = 0;
    uint64_t now_ms = 0;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    now_ms = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    if (last_ms != 0) {
        printf("Cyclic task executed, delta: %llu ms\n", (unsigned long long)(now_ms - last_ms));
    } else {
        printf("Cyclic task executed, first call\n");
    }
    last_ms = now_ms;
}

int main() {
    int tfd;
    struct itimerspec timer;
    uint64_t expirations;

    tfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (tfd == -1) {
        perror("timerfd_create");
        return 1;
    }

    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_nsec = 250 * 1000000; // 250ms
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_nsec = 250 * 1000000; // first expiration after 250ms

    if (timerfd_settime(tfd, 0, &timer, NULL) == -1) {
        perror("timerfd_settime");
        close(tfd);
        return 1;
    }

    while (1) {
        if (read(tfd, &expirations, sizeof(expirations)) != sizeof(expirations)) {
            perror("read");
            break;
        }
        cyclic_task();
    }

    close(tfd);
    return 0;
}