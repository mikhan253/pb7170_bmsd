#ifndef SPI_H
#define SPI_H

extern int g_spiFd;

int spi_SelectDevice(uint_fast8_t device);
int spi_Init(const char *spiDevice, uint32_t speed, uint8_t mode, uint8_t bits, const char *gpioDevice, const unsigned int *gpioPins, const unsigned int gpioNrPins);
int spi_AFEReadRegister(uint8_t addr, uint16_t* output, uint_fast8_t count);
int spi_AFEWriteRegister(uint8_t addr, uint16_t data);
void spi_Cleanup(void);

#endif