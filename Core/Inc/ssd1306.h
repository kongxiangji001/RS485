#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>
#include "i2c.h"

#define SSD1306_I2C_ADDR 0x3CU
#define SSD1306_WIDTH    128U
#define SSD1306_HEIGHT   64U
#define SSD1306_PAGES    8U

void SSD1306_Init(void);
void SSD1306_Clear(void);
void SSD1306_WriteString(uint8_t x, uint8_t y, const char *str);
void SSD1306_Update(void);

#endif /* SSD1306_H */
