#ifndef SPI_H
#define SPI_H

extern int g_spiFd;

int spi_SelectDevice(uint_fast8_t device);
int spi_Init(const char *device, uint32_t speed, uint8_t mode, uint8_t bits);
int spi_AFEReadRegister(uint8_t addr, uint16_t* output, uint_fast8_t count);
int spi_AFEWriteRegister(uint8_t addr, uint16_t data);

#endif