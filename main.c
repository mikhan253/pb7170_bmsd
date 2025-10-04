#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <sys/timerfd.h>
#include <sys/resource.h>
#include <sched.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <gpiod.h>

#include "main.h"


// Name und Größe des Shared Memory
#define SHMEM_NAME "/battery_pdo_shmem"
#define SHMEM_SIZE (sizeof(BATTERY_PDO_t) * NUMBER_OF_BATTERY_PACKS)


BATTERY_PDO_t *pdo_array = NULL;

volatile sig_atomic_t terminate = 0;
int tfd = -1;
int spi_fd = -1;

void handle_signal(int sig) {
    terminate = 1;
}

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

int setup_spi() {
    const char *device = "/dev/spidev0.0";
    uint8_t mode = SPI_MODE_0;
    uint8_t bits = 8;
    uint32_t speed = 500000;

    spi_fd = open(device, O_RDWR);
    if (spi_fd < 0) {
        perror("open spidev");
        return -1;
    }
    if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) == -1) {
        perror("SPI_IOC_WR_MODE");
        return -1;
    }
    if (ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) == -1) {
        perror("SPI_IOC_WR_BITS_PER_WORD");
        return -1;
    }
    if (ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) == -1) {
        perror("SPI_IOC_WR_MAX_SPEED_HZ");
        return -1;
    }
    return 0;
}
int setup_signal_handlers() {
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction SIGINT");
        return -1;
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction SIGTERM");
        return -1;
    }
    return 0;
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
    pdo_array = mmap(NULL, SHMEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
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

int setup_timerfd(int interval_ms) {
    tfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (tfd == -1) {
        perror("timerfd_create");
        return -1;
    }

    struct itimerspec new_value;
    new_value.it_interval.tv_sec = interval_ms / 1000;
    new_value.it_interval.tv_nsec = (interval_ms % 1000) * 1000000;
    new_value.it_value.tv_sec = interval_ms / 1000;
    new_value.it_value.tv_nsec = (interval_ms % 1000) * 1000000;

    if (timerfd_settime(tfd, 0, &new_value, NULL) == -1) {
        perror("timerfd_settime");
        close(tfd);
        return -1;
    }
    return 0;
}

int main() {

    if (setup_signal_handlers() != 0) {
        printf("Failed to setup signal handlers\n");
        return 1;
    }
    if (setup_process() != 0) {
        printf("Failed to setup process\n");
        return 1;
    }
    if (setup_shared_memory() != 0) {
        printf("Failed to setup shared memory\n");
        return 1;
    }

    if (setup_timerfd(250) != 0) {
        printf("Failed to setup timerfd\n");
        return 1;
    }

    // tfd ist jetzt global
    uint64_t expirations;


    while (!terminate) {
        if (read(tfd, &expirations, sizeof(expirations)) != sizeof(expirations)) {
            if (errno == EINTR && terminate) break;
            perror("read");
            break;
        }
        cyclic_task();
    }

    if (tfd != -1) close(tfd);

    // Shared Memory freigeben
    if (pdo_array) {
        munmap(pdo_array, SHMEM_SIZE);
    }
    shm_unlink(SHMEM_NAME);

    printf("Programm beendet und Ressourcen freigegeben.\n");
    return 0;
}

    // ...existing code...
    // GPIO freigeben
    if (line) gpiod_line_release(line);
    if (chip) gpiod_chip_close(chip);
    // ...existing code...


int spi_transfer(uint8_t tx, uint8_t *rx) {
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)&tx,
        .rx_buf = (unsigned long)rx,
        .len = 1,
        .speed_hz = 500000,
        .bits_per_word = 8,
    };
    int ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 1) {
        perror("SPI_IOC_MESSAGE");
        return -1;
    }
    return 0;
}

struct gpiod_chip *chip = NULL;
struct gpiod_line *line = NULL;

int setup_gpio() {
    chip = gpiod_chip_open_by_name("gpiochip0"); // Name ggf. anpassen
    if (!chip) {
        perror("gpiod_chip_open_by_name");
        return -1;
    }
    line = gpiod_chip_get_line(chip, 17); // z.B. GPIO17
    if (!line) {
        perror("gpiod_chip_get_line");
        gpiod_chip_close(chip);
        return -1;
    }
    if (gpiod_line_request_output(line, "pb7170_bmsd", 0) < 0) {
        perror("gpiod_line_request_output");
        gpiod_chip_close(chip);
        return -1;
    }
    return 0;
}

void set_gpio(int value) {
    if (line) {
        gpiod_line_set_value(line, value);
    }
}

// ...existing code...
struct gpiod_chip *chip = NULL;
struct gpiod_line_bulk bulk;

// GPIO-Nummern, die du gemeinsam steuern willst
const unsigned int gpios[] = {17, 18, 27}; // Beispiel: GPIO17, GPIO18, GPIO27

int setup_gpio_bulk() {
    chip = gpiod_chip_open_by_name("gpiochip0");
    if (!chip) {
        perror("gpiod_chip_open_by_name");
        return -1;
    }
    if (gpiod_chip_get_lines(chip, gpios, 3, &bulk) < 0) {
        perror("gpiod_chip_get_lines");
        gpiod_chip_close(chip);
        return -1;
    }
    int values[3] = {0, 0, 0}; // Initial alle auf LOW
    if (gpiod_line_request_output_lines(&bulk, "pb7170_bmsd", values) < 0) {
        perror("gpiod_line_request_output_lines");
        gpiod_chip_close(chip);
        return -1;
    }
    return 0;
}

// Setzt alle GPIOs auf die gewünschten Werte (z.B. {1,0,1})
void set_gpio_bulk(const int *values) {
    if (bulk.num_lines > 0) {
        gpiod_line_set_value_bulk(&bulk, values);
    }
}
// ...existing code...