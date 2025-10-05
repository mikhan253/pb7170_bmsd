#ifndef SPI_H
#define SPI_H

extern int spi_fd;

int spi_select_device(uint_fast8_t device);
int spi_init(const char *device, uint32_t speed, uint8_t mode, uint8_t bits);
int pb7170_spi_read_register(uint8_t reg_addr, uint16_t* output, uint_fast8_t count);
int pb7170_spi_write_register(uint8_t reg_addr, uint16_t data);

#endif