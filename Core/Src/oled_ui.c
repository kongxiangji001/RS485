#include "oled_ui.h"

#include "ssd1306.h"
#include "usart.h"

static void OLED_UI_WriteCount(uint8_t y, const char *label, volatile uint32_t *count)
{
  char line[16];
  uint32_t value = *count;
  uint8_t digit;
  uint8_t label_length = 0U;

  while (label[label_length] != '\0')
  {
    line[label_length] = label[label_length];
    label_length++;
  }

  for (digit = 0U; digit < 9U; digit++)
  {
    line[label_length + 8U - digit] = (char)('0' + (value % 10U));
    value /= 10U;
  }
  line[label_length + 9U] = '\0';

  SSD1306_WriteString(0U, y, line);
}

void OLED_UI_Update(void)
{
  SSD1306_Clear();
  SSD1306_WriteString(0U, 0U, "RS485 MONITOR");
  SSD1306_WriteString(0U, 8U, "--------------------");
  OLED_UI_WriteCount(17U, "U1:", &uart1_rx_count);
  OLED_UI_WriteCount(26U, "U2:", &uart2_rx_count);
  OLED_UI_WriteCount(35U, "U3:", &uart3_rx_count);
  OLED_UI_WriteCount(44U, "TOTAL:", &uart_total_rx_count);
  OLED_UI_WriteCount(53U, "DROP:", &uart_rx_queue_drops);
  SSD1306_Update();
}
