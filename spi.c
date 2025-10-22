#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <gpiod.h>

// ---------------- Globals ----------------
int g_spiFd = -1;                        // globales SPI-Filedescriptor
static uint8_t s_spiTxBuf[67];          // globaler TX-Buffer
static uint8_t s_spiRxBuf[67];          // globaler RX-Buffer
static struct spi_ioc_transfer s_SpiTr;  // globaler SPI-Transfer struct

static unsigned int s_gpioNrPins;
static struct gpiod_line_request *s_gpioRequest;

// ---------------- CRC8 ----------------
static const uint8_t s_afeCrc8Table[256] =
{
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15, 0x38, 0x3F, 0x36, 0x31, 
    0x24, 0x23, 0x2A, 0x2D, 0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65, 
    0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D, 0xE0, 0xE7, 0xEE, 0xE9, 
    0xFC, 0xFB, 0xF2, 0xF5, 0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD, 
    0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85, 0xA8, 0xAF, 0xA6, 0xA1, 
    0xB4, 0xB3, 0xBA, 0xBD, 0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2, 
    0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA, 0xB7, 0xB0, 0xB9, 0xBE, 
    0xAB, 0xAC, 0xA5, 0xA2, 0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A, 
    0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32, 0x1F, 0x18, 0x11, 0x16, 
    0x03, 0x04, 0x0D, 0x0A, 0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42, 
    0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A, 0x89, 0x8E, 0x87, 0x80, 
    0x95, 0x92, 0x9B, 0x9C, 0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4, 
    0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC, 0xC1, 0xC6, 0xCF, 0xC8, 
    0xDD, 0xDA, 0xD3, 0xD4, 0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C, 
    0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44, 0x19, 0x1E, 0x17, 0x10, 
    0x05, 0x02, 0x0B, 0x0C, 0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34, 
    0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B, 0x76, 0x71, 0x78, 0x7F, 
    0x6A, 0x6D, 0x64, 0x63, 0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B, 
    0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13, 0xAE, 0xA9, 0xA0, 0xA7, 
    0xB2, 0xB5, 0xBC, 0xBB, 0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83, 
    0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB, 0xE6, 0xE1, 0xE8, 0xEF, 
    0xFA, 0xFD, 0xF4, 0xF3
};

static inline uint8_t AFECrc8(const void *data, uint_fast8_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint8_t crc = 0x00;

    while (len >= 4)
    {
        crc = s_afeCrc8Table[crc ^ *p++];
        crc = s_afeCrc8Table[crc ^ *p++];
        crc = s_afeCrc8Table[crc ^ *p++];
        crc = s_afeCrc8Table[crc ^ *p++];
        len -= 4;
    }
    while (len--)
    {
        crc = s_afeCrc8Table[crc ^ *p++];
    }
    return crc;
}

// ---------------- SPI Functions ----------------
int spi_SelectDevice(uint_fast8_t spiDevice)
{
    enum gpiod_line_value values[s_gpioNrPins];
    for (int i = 0; i < s_gpioNrPins; i++)
        values[i] = (spiDevice >> i) & 1;
    
    return gpiod_line_request_set_values(s_gpioRequest, values);
}

//static const unsigned int s_gpioAddrPins[NUM_ADDR_PINS] = {24, 25, 26};
//static const char *const s_gpioChipPath = "/dev/gpiochip1";

int spi_Init(const char *spiDevice, uint32_t speed, uint8_t mode, uint8_t bits, const char *gpioDevice, const unsigned int *gpioPins, const unsigned int gpioNrPins)
{
    // SPI Init
    s_SpiTr.tx_buf = (unsigned long)s_spiTxBuf;
    s_SpiTr.rx_buf = (unsigned long)s_spiRxBuf;
    s_SpiTr.delay_usecs = 0;
    s_SpiTr.cs_change = 0;
    s_SpiTr.speed_hz = speed;
    s_SpiTr.bits_per_word = bits;

    g_spiFd = open(spiDevice, O_RDWR);
    if (g_spiFd < 0) 
        return -1;
    if (ioctl(g_spiFd, SPI_IOC_WR_MODE, &mode) < 0)
        goto error;
    if (ioctl(g_spiFd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0)
        goto error;
    if (ioctl(g_spiFd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0)
        goto error;

    // GPIO Init
    s_gpioNrPins = gpioNrPins;

    struct gpiod_chip *chip;
    chip = gpiod_chip_open(gpioDevice);
	if (!chip)
		goto error;

    struct gpiod_line_settings *settings;
    settings = gpiod_line_settings_new();
	if (!settings)
		goto error_chip;
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);

    struct gpiod_line_config *lconfig;
    lconfig = gpiod_line_config_new();
	if (!lconfig)
		goto error_linesettings;
    for (uint32_t i = 0; i < gpioNrPins; i++)
		if (gpiod_line_config_add_line_settings(lconfig, &gpioPins[i], 1, settings))
			goto error_lineconfig;

    struct gpiod_request_config *rconfig = NULL;
    rconfig = gpiod_request_config_new();
    if (!rconfig)
        goto error_lineconfig;
    gpiod_request_config_set_consumer(rconfig, "bmsd");

    s_gpioRequest = gpiod_chip_request_lines(chip, rconfig, lconfig);
    if (!s_gpioRequest)
        goto error_lineconfig;
    
    return 0;
error_lineconfig:
    gpiod_line_config_free(lconfig);
error_linesettings:
    gpiod_line_settings_free(settings);
error_chip:
	gpiod_chip_close(chip);
error:
    close(g_spiFd);
    return -1;
}

int spi_AFEReadRegister(uint8_t addr, uint16_t* output, uint_fast8_t count) 
{
    if (count > 32) 
        return -1; // Maximale Anzahl überschritten

    size_t tx_len = 2 + (size_t)count * 2 + 1; // 2 Byte Kommando + count*2 Byte Daten + 1 Byte CRC
    s_SpiTr.len = tx_len;

    // Header setzen
    s_spiTxBuf[0] = (addr & 0x7F) << 1;
    s_spiTxBuf[1] = ((count - 1) & 0x1F) | (addr & 0x80);

    // SPI-Transfer durchführen
    if (ioctl(g_spiFd, SPI_IOC_MESSAGE(1), &s_SpiTr) < 0)
        return -1; // SPI Fehler

        // Header für CRC prüfen
    s_spiRxBuf[0] = s_spiTxBuf[0];
    s_spiRxBuf[1] = s_spiTxBuf[1];
    if (AFECrc8(s_spiRxBuf, tx_len))
        return -3; // CRC Fehler

    // Pointer-basiert Daten aus RX extrahieren (big-endian)
    const uint8_t *p = &s_spiRxBuf[2];
    for (uint_fast8_t i = 0; i < count; i++)
    {
        output[i] = ((uint16_t)p[0] << 8) | p[1];
        p += 2;
    }

    return 0; // Erfolg
}

int spi_AFEWriteRegister(uint8_t addr, uint16_t data) 
{
    s_SpiTr.len = 4;

    // Header setzen
    s_spiTxBuf[0] = ((addr & 0x7F) << 1) | 1;
    // Daten setzen
    s_spiTxBuf[1] = data >> 8;
    s_spiTxBuf[2] = data & 0xFF;
    // CRC setzen
    s_spiTxBuf[3] = AFECrc8(s_spiTxBuf, 3);

    // SPI-Transfer durchführen
    if (ioctl(g_spiFd, SPI_IOC_MESSAGE(1), &s_SpiTr) < 0)
        return -1; // SPI Fehler

    return 0; // Erfolg
}

void spi_Cleanup(void)
{
    // GPIO-Ressourcen freigeben
    if (s_gpioRequest)
    {
        gpiod_line_request_release(s_gpioRequest);
        s_gpioRequest = NULL;
    }
    
    // SPI-Filedescriptor schließen
    if (g_spiFd >= 0)
    {
        close(g_spiFd);
        g_spiFd = -1;
    }
    
    // Globale Variablen zurücksetzen
    s_gpioNrPins = 0;
}