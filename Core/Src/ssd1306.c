#include "ssd1306.h"

#include <string.h>

static uint8_t ssd1306_buffer[SSD1306_WIDTH * SSD1306_PAGES];

static HAL_StatusTypeDef SSD1306_WriteCommand(uint8_t cmd)
{
  uint8_t buf[1];
  buf[0] = cmd;
  return HAL_I2C_Mem_Write(&hi2c1, (SSD1306_I2C_ADDR << 1U), 0x00U, I2C_MEMADD_SIZE_8BIT, buf, 1U, 1000U);
}

static HAL_StatusTypeDef SSD1306_WriteData(uint8_t *pData, uint16_t len)
{
  return HAL_I2C_Mem_Write(&hi2c1, (SSD1306_I2C_ADDR << 1U), 0x40U, I2C_MEMADD_SIZE_8BIT, pData, len, 1000U);
}

static void SSD1306_SetCursor(uint8_t page, uint8_t col)
{
  SSD1306_WriteCommand(0xB0U | (page & 0x07U));
  SSD1306_WriteCommand(0x10U | ((col >> 4U) & 0x0FU));
  SSD1306_WriteCommand(col & 0x0FU);
}

static void SSD1306_DrawPixel(uint8_t x, uint8_t y)
{
  if ((x >= SSD1306_WIDTH) || (y >= SSD1306_HEIGHT))
  {
    return;
  }
  ssd1306_buffer[(y / 8U) * SSD1306_WIDTH + x] |= (uint8_t)(1U << (y % 8U));
}

static void SSD1306_DrawChar(uint8_t x, uint8_t y, char ch)
{
  uint8_t row;
  uint8_t col;
  const uint8_t *font = NULL;
  static const uint8_t font_space[5] = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
  static const uint8_t font_A[5] = {0x7EU, 0x11U, 0x11U, 0x11U, 0x7EU};
  static const uint8_t font_B[5] = {0x7CU, 0x4AU, 0x49U, 0x49U, 0x36U};
  static const uint8_t font_C[5] = {0x3EU, 0x41U, 0x41U, 0x41U, 0x22U};
  static const uint8_t font_D[5] = {0x7FU, 0x41U, 0x41U, 0x22U, 0x1CU};
  static const uint8_t font_E[5] = {0x7FU, 0x49U, 0x49U, 0x49U, 0x41U};
  static const uint8_t font_I[5] = {0x00U, 0x41U, 0x7FU, 0x41U, 0x00U};
  static const uint8_t font_L[5] = {0x00U, 0x40U, 0x40U, 0x40U, 0x7FU};
  static const uint8_t font_N[5] = {0x7FU, 0x08U, 0x06U, 0x01U, 0x7FU};
  static const uint8_t font_M[5] = {0x7FU, 0x02U, 0x0CU, 0x02U, 0x7FU};
  static const uint8_t font_O[5] = {0x3EU, 0x41U, 0x41U, 0x41U, 0x3EU};
  static const uint8_t font_P[5] = {0x7FU, 0x09U, 0x09U, 0x09U, 0x06U};
  static const uint8_t font_R[5] = {0x7FU, 0x09U, 0x19U, 0x29U, 0x46U};
  static const uint8_t font_S[5] = {0x46U, 0x49U, 0x49U, 0x49U, 0x31U};
  static const uint8_t font_T[5] = {0x01U, 0x01U, 0x7FU, 0x01U, 0x01U};
  static const uint8_t font_U[5] = {0x3FU, 0x40U, 0x40U, 0x40U, 0x3FU};
  static const uint8_t font_V[5] = {0x1FU, 0x20U, 0x40U, 0x20U, 0x1FU};
  static const uint8_t font_0[5] = {0x3EU, 0x41U, 0x41U, 0x41U, 0x3EU};
  static const uint8_t font_1[5] = {0x00U, 0x42U, 0x7FU, 0x40U, 0x00U};
  static const uint8_t font_2[5] = {0x62U, 0x51U, 0x49U, 0x49U, 0x46U};
  static const uint8_t font_3[5] = {0x22U, 0x41U, 0x49U, 0x49U, 0x36U};
  static const uint8_t font_4[5] = {0x18U, 0x14U, 0x12U, 0x7FU, 0x10U};
  static const uint8_t font_5[5] = {0x39U, 0x49U, 0x49U, 0x49U, 0x30U};
  static const uint8_t font_6[5] = {0x3EU, 0x49U, 0x49U, 0x49U, 0x30U};
  static const uint8_t font_7[5] = {0x01U, 0x71U, 0x09U, 0x05U, 0x03U};
  static const uint8_t font_8[5] = {0x36U, 0x49U, 0x49U, 0x49U, 0x36U};
  static const uint8_t font_9[5] = {0x30U, 0x49U, 0x49U, 0x49U, 0x3EU};
  static const uint8_t font_colon[5] = {0x00U, 0x36U, 0x36U, 0x00U, 0x00U};
  static const uint8_t font_hyphen[5] = {0x08U, 0x08U, 0x08U, 0x08U, 0x08U};
  static const uint8_t font_X[5] = {0x63U, 0x14U, 0x08U, 0x14U, 0x63U};

  if ((x > SSD1306_WIDTH - 6U) || (y > SSD1306_HEIGHT - 8U))
  {
    return;
  }

  switch (ch)
  {
    case ' ': font = font_space; break;
    case 'A': font = font_A; break;
    case 'B': font = font_B; break;
    case 'C': font = font_C; break;
    case 'D': font = font_D; break;
    case 'E': font = font_E; break;
    case 'I': font = font_I; break;
    case 'L': font = font_L; break;
    case 'N': font = font_N; break;
    case 'M': font = font_M; break;
    case 'O': font = font_O; break;
    case 'P': font = font_P; break;
    case 'R': font = font_R; break;
    case 'S': font = font_S; break;
    case 'T': font = font_T; break;
    case 'U': font = font_U; break;
    case 'V': font = font_V; break;
    case '0': font = font_0; break;
    case '1': font = font_1; break;
    case '2': font = font_2; break;
    case '3': font = font_3; break;
    case '4': font = font_4; break;
    case '5': font = font_5; break;
    case '6': font = font_6; break;
    case '7': font = font_7; break;
    case '8': font = font_8; break;
    case '9': font = font_9; break;
    case ':': font = font_colon; break;
    case '-': font = font_hyphen; break;
    case 'X': font = font_X; break;
    default: font = font_space; break;
  }

  for (col = 0U; col < 5U; col++)
  {
    uint8_t mask = font[col];
    for (row = 0U; row < 8U; row++)
    {
      if (((mask >> row) & 0x01U) != 0U)
      {
        SSD1306_DrawPixel(x + col, y + row);
      }
    }
  }
}

void SSD1306_WriteString(uint8_t x, uint8_t y, const char *str)
{
  uint8_t i = 0U;
  while (str[i] != '\0')
  {
    SSD1306_DrawChar(x + i * 6U, y, str[i]);
    i++;
  }
}

void SSD1306_Clear(void)
{
  memset(ssd1306_buffer, 0x00, sizeof(ssd1306_buffer));
}

void SSD1306_Update(void)
{
  uint8_t page;
  uint8_t col;
  uint8_t temp[SSD1306_WIDTH];

  for (page = 0U; page < SSD1306_PAGES; page++)
  {
    SSD1306_SetCursor(page, 0U);
    for (col = 0U; col < SSD1306_WIDTH; col++)
    {
      temp[col] = ssd1306_buffer[(page * SSD1306_WIDTH) + col];
    }
    SSD1306_WriteData(temp, SSD1306_WIDTH);
  }
}

void SSD1306_Init(void)
{
  HAL_Delay(100U);
  SSD1306_WriteCommand(0xAE);
  SSD1306_WriteCommand(0xD5);
  SSD1306_WriteCommand(0x80);
  SSD1306_WriteCommand(0xA8);
  SSD1306_WriteCommand(0x3F);
  SSD1306_WriteCommand(0xD3);
  SSD1306_WriteCommand(0x00);
  SSD1306_WriteCommand(0x40);
  SSD1306_WriteCommand(0x8D);
  SSD1306_WriteCommand(0x14);
  SSD1306_WriteCommand(0x20);
  SSD1306_WriteCommand(0x00);
  SSD1306_WriteCommand(0xA1);
  SSD1306_WriteCommand(0xC8);
  SSD1306_WriteCommand(0xDA);
  SSD1306_WriteCommand(0x12);
  SSD1306_WriteCommand(0x81);
  SSD1306_WriteCommand(0x7F);
  SSD1306_WriteCommand(0xD9);
  SSD1306_WriteCommand(0xF1);
  SSD1306_WriteCommand(0xDB);
  SSD1306_WriteCommand(0x40);
  SSD1306_WriteCommand(0xA4);
  SSD1306_WriteCommand(0xA6);
  SSD1306_WriteCommand(0xAF);

  SSD1306_Clear();
  SSD1306_Update();
}
