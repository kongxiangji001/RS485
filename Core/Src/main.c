/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "i2c.h"
#include "spi.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"
#include "usbd_cdc_if.h"
#include <string.h>

#define SSD1306_I2C_ADDR 0x3CU
#define SSD1306_WIDTH    128U
#define SSD1306_HEIGHT   64U
#define SSD1306_PAGES    8U

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
  static const uint8_t font_space[5] = {0x00,0x00,0x00,0x00,0x00};
  static const uint8_t font_A[5] = {0x7E,0x11,0x11,0x11,0x7E};
  static const uint8_t font_C[5] = {0x3E,0x41,0x41,0x41,0x22};
  static const uint8_t font_E[5] = {0x7F,0x49,0x49,0x49,0x41};
  static const uint8_t font_I[5] = {0x00,0x41,0x7F,0x41,0x00};
  static const uint8_t font_L[5] = {0x00,0x40,0x40,0x40,0x7F};
  static const uint8_t font_N[5] = {0x7F,0x08,0x06,0x01,0x7F};
  static const uint8_t font_R[5] = {0x7F,0x09,0x19,0x29,0x46};
  static const uint8_t font_S[5] = {0x46,0x49,0x49,0x49,0x31};
  static const uint8_t font_T[5] = {0x01,0x01,0x7F,0x01,0x01};
  static const uint8_t font_U[5] = {0x3F,0x40,0x40,0x40,0x3F};
  static const uint8_t font_V[5] = {0x1F,0x20,0x40,0x20,0x1F};
  static const uint8_t font_0[5] = {0x3E,0x41,0x41,0x41,0x3E};
  static const uint8_t font_1[5] = {0x00,0x42,0x7F,0x40,0x00};
  static const uint8_t font_2[5] = {0x62,0x51,0x49,0x49,0x46};
  static const uint8_t font_3[5] = {0x22,0x41,0x49,0x49,0x36};
  static const uint8_t font_4[5] = {0x18,0x14,0x12,0x7F,0x10};
  static const uint8_t font_5[5] = {0x39,0x49,0x49,0x49,0x30};
  static const uint8_t font_6[5] = {0x3E,0x49,0x49,0x49,0x30};
  static const uint8_t font_8[5] = {0x36,0x49,0x49,0x49,0x36};
  static const uint8_t font_9[5] = {0x30,0x49,0x49,0x49,0x3E};

  if ((x > SSD1306_WIDTH - 6U) || (y > SSD1306_HEIGHT - 8U))
  {
    return;
  }

  switch (ch)
  {
    case ' ': font = font_space; break;
    case 'A': font = font_A; break;
    case 'C': font = font_C; break;
    case 'E': font = font_E; break;
    case 'I': font = font_I; break;
    case 'L': font = font_L; break;
    case 'N': font = font_N; break;
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
    case '8': font = font_8; break;
    case '9': font = font_9; break;
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

static void SSD1306_WriteString(uint8_t x, uint8_t y, const char *str)
{
  uint8_t i = 0U;
  while (str[i] != '\0')
  {
    SSD1306_DrawChar(x + i * 6U, y, str[i]);
    i++;
  }
}

static void SSD1306_Clear(void)
{
  memset(ssd1306_buffer, 0x00, sizeof(ssd1306_buffer));
}

static void SSD1306_Update(void)
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

static void SSD1306_Init(void)
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_USB_DEVICE_Init();
  MX_USART_DMA_Receive_Start();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */
  SSD1306_Init();
  SSD1306_Clear();
  SSD1306_WriteString(0U, 0U, "RS485");
  SSD1306_WriteString(0U, 16U, "LISTEN");
  SSD1306_Update();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    HAL_Delay(1000U);
    SSD1306_Clear();
    SSD1306_WriteString(0U, 0U, "RS485");
    SSD1306_WriteString(0U, 16U, "ACTIVE");
    SSD1306_Update();
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_RCC_MCOConfig(RCC_MCO, RCC_MCO1SOURCE_PLLCLK, RCC_MCODIV_1);
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
