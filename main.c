#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

#include "tasksetup.h"

int spi_fd = -1;

int main(void) {

    if (setup_task(250)) {
        printf("Failed to set up task\n");
        return 1;
    }

    while (1) {
        uint64_t expirations;
        read(timer_fd, &expirations, sizeof(expirations));  // blockiert bis Timer feuert
        if (expirations == 1)
        {
            printf("Timer expired once\n");
        }
        else
            printf("Timer expired %llu times\n", (unsigned long long)expirations);
    }

    close(timer_fd);
    return 0;
}
