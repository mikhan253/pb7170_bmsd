#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

#include "tasksetup.h"
#include "spi.h"

int main(void) {
    uint16_t data;

    if (setup_task(250)) {
        printf("Failed to set up task\n");
        return 1;
    }
    if (spi_init("/dev/spidev0.0", 1000000, 0, 8)) {
        printf("Failed to initialize SPI\n");
        return 1;
    }

    while (1) {
        uint64_t expirations;
        read(timer_fd, &expirations, sizeof(expirations));  // blockiert bis Timer feuert
        if (expirations == 1)
        {
            pb7170_spi_read_register(0x01, &data, 1);
            printf("Timer expired once: 0x%04X\n", data);
        }
        else
            printf("Timer expired %llu times\n", (unsigned long long)expirations);
    }

    close(timer_fd);
    return 0;
}
