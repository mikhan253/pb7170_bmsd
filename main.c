#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

#include <sys/timerfd.h>
#include <sys/resource.h>
#include <sched.h>
#include <sys/mman.h>
#include <fcntl.h>


#include "main.h"


// Name und Größe des Shared Memory
#define SHMEM_NAME "/battery_pdo_shmem"
#define SHMEM_SIZE (sizeof(BATTERY_PDO_t) * 8)


BATTERY_PDO_t *pdo_array = NULL;


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

int setup_process() {
    // Setze sehr hohe Prozess-Priorität (Realtime)
    if (setpriority(PRIO_PROCESS, 0, -20) != 0) {
        perror("setpriority");
        return -1;
    }

    struct sched_param param;
    param.sched_priority = 99;
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        perror("sched_setscheduler");
        return -1;
    }

    // Verhindere OOM-Kill
    FILE *oom = fopen("/proc/self/oom_score_adj", "w");
    if (oom) {
        fprintf(oom, "-1000\n");
        fclose(oom);
    } else {
        perror("oom_score_adj");
        return -1;
    }
    return 0;
}

int setup_shared_memory() {

    int shm_fd = shm_open(SHMEM_NAME, O_CREAT | O_RDWR, 0666);

    if (shm_fd == -1) {
        perror("shm_open");
        return -1;
    }
    if (ftruncate(shm_fd, SHMEM_SIZE) == -1) {
        perror("ftruncate");
        close(shm_fd);
        return -1;
    }
    pdo_array = mmap(NULL, SHMEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)
    if (pdo_array == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        return -1;
    }

    for (int i = 0; i < 8; ++i) {
        pdo_array[i].ID = i;
        pdo_array[i].Statemachine = PB7170_STATE_WAIT_INIT;
    }
    return 0;
}

int main() {

    if (setup_process() != 0) {
        printf("Failed to setup process\n");
        return 1;
    }
    if (setup_shared_memory() != 0) {
        printf("Failed to setup shared memory\n");
        return 1;
    }


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