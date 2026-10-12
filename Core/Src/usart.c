/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
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
#include "usart.h"
#include "usbd_cdc_if.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;
DMA_HandleTypeDef hdma_usart2_rx;
DMA_HandleTypeDef hdma_usart2_tx;
DMA_HandleTypeDef hdma_usart3_rx;
DMA_HandleTypeDef hdma_usart3_tx;

uint8_t uart1_rx_buf[UART_RX_BUF_SIZE];
uint8_t uart2_rx_buf[UART_RX_BUF_SIZE];
uint8_t uart3_rx_buf[UART_RX_BUF_SIZE];
volatile uint32_t uart1_rx_count;
volatile uint32_t uart2_rx_count;
volatile uint32_t uart3_rx_count;
volatile uint32_t uart_total_rx_count;
volatile uint32_t uart_rx_queue_drops;

#define UART_RECORD_QUEUE_SIZE 201U
#define UART_RECORD_DATA_SIZE 32U
#define UART_BATCH_SIZE       100U
#define UART_COUNT_MAX        999999999UL
#define UART_USB_LINE_SIZE    160U

typedef struct
{
  uint32_t sequence;
  uint32_t batch;
  uint8_t data[UART_RECORD_DATA_SIZE];
  uint16_t received_length;
  uint8_t port;
  uint8_t oversized;
} UART_Record;

static UART_Record uart_record_queue[UART_RECORD_QUEUE_SIZE];
static volatile uint16_t uart_queue_head;
static volatile uint16_t uart_queue_tail;
static uint32_t uart_current_batch = 1U;
static volatile uint32_t uart_completed_batches;
static uint16_t uart_batch_receive_count;
static uint32_t uart_usb_active_batch = 1U;
static uint8_t uart_usb_tx_in_flight;
static uint8_t uart_usb_tx_line[UART_USB_LINE_SIZE];

static uint16_t UART_Queue_Next(uint16_t index)
{
  index++;
  if (index >= UART_RECORD_QUEUE_SIZE)
  {
    index = 0U;
  }
  return index;
}

static void UART_Count_Increment(volatile uint32_t *count)
{
  if (*count < UART_COUNT_MAX)
  {
    (*count)++;
  }
}

static void UART_Receive_Start_Once(UART_HandleTypeDef *huart, uint8_t *buf, uint16_t size)
{
  if (HAL_UARTEx_ReceiveToIdle_DMA(huart, buf, size) != HAL_OK)
  {
    Error_Handler();
  }
  if (huart->hdmarx != NULL)
  {
    __HAL_DMA_DISABLE_IT(huart->hdmarx, DMA_IT_HT);
  }
}

static uint8_t UART_Port_Number(UART_HandleTypeDef *huart)
{
  if (huart == &huart1)
  {
    return 1U;
  }
  else if (huart == &huart2)
  {
    return 2U;
  }
  else if (huart == &huart3)
  {
    return 3U;
  }
  return 0U;
}

static void UART_Record_Received(UART_HandleTypeDef *huart, uint8_t *buf, uint16_t len)
{
  uint16_t next_head;
  uint8_t port;
  UART_Record *record;

  if (len == 0U)
  {
    return;
  }

  port = UART_Port_Number(huart);
  if (port == 0U)
  {
    return;
  }

  UART_Count_Increment(&uart_total_rx_count);
  if (port == 1U)
  {
    UART_Count_Increment(&uart1_rx_count);
  }
  else if (port == 2U)
  {
    UART_Count_Increment(&uart2_rx_count);
  }
  else
  {
    UART_Count_Increment(&uart3_rx_count);
  }

  next_head = UART_Queue_Next(uart_queue_head);
  if (next_head == uart_queue_tail)
  {
    UART_Count_Increment(&uart_rx_queue_drops);
  }
  else
  {
    record = &uart_record_queue[uart_queue_head];
    record->sequence = uart_total_rx_count;
    record->batch = uart_current_batch;
    record->received_length = len;
    record->port = port;
    record->oversized = (len > UART_RECORD_DATA_SIZE) ? 1U : 0U;
    if (record->oversized == 0U)
    {
      memcpy(record->data, buf, len);
    }
    __DMB();
    uart_queue_head = next_head;
  }

  uart_batch_receive_count++;
  if (uart_batch_receive_count >= UART_BATCH_SIZE)
  {
    uart_batch_receive_count = 0U;
    uart_completed_batches = uart_current_batch;
    if (uart_current_batch < 0xFFFFFFFFUL)
    {
      uart_current_batch++;
    }
  }
}

static uint16_t UART_Append_Decimal(uint8_t *out, uint16_t pos, uint32_t value)
{
  uint8_t digits[10];
  uint8_t count = 0U;

  do
  {
    digits[count++] = (uint8_t)('0' + (value % 10U));
    value /= 10U;
  } while (value != 0U);

  while (count > 0U)
  {
    out[pos++] = digits[--count];
  }
  return pos;
}

static uint16_t UART_Format_Record(const UART_Record *record, uint8_t *out)
{
  static const uint8_t hex[] = "0123456789ABCDEF";
  uint16_t pos = 0U;
  uint16_t index;

  pos = UART_Append_Decimal(out, pos, record->sequence);
  out[pos++] = '\t';
  out[pos++] = 'U';
  out[pos++] = (uint8_t)('0' + record->port);
  out[pos++] = '\t';

  if (record->oversized != 0U)
  {
    static const uint8_t oversized[] = "OVERSIZE(";
    memcpy(&out[pos], oversized, sizeof(oversized) - 1U);
    pos += sizeof(oversized) - 1U;
    pos = UART_Append_Decimal(out, pos, record->received_length);
    out[pos++] = ')';
  }
  else
  {
    for (index = 0U; index < record->received_length; index++)
    {
      uint8_t value = record->data[index];
      if ((value >= 0x20U) && (value <= 0x7EU) && (value != '\\'))
      {
        out[pos++] = value;
      }
      else
      {
        out[pos++] = '\\';
        out[pos++] = 'x';
        out[pos++] = hex[value >> 4U];
        out[pos++] = hex[value & 0x0FU];
      }
    }
  }

  out[pos++] = '\r';
  out[pos++] = '\n';
  return pos;
}

void UART_USB_Process(void)
{
  UART_Record *record;
  uint16_t line_length;

  if (uart_usb_tx_in_flight != 0U)
  {
    if (CDC_TransmitReady_FS() == 0U)
    {
      return;
    }
    uart_usb_tx_in_flight = 0U;
    __DMB();
    uart_queue_tail = UART_Queue_Next(uart_queue_tail);
  }

  while (uart_usb_active_batch <= uart_completed_batches)
  {
    if (uart_queue_tail == uart_queue_head)
    {
      uart_usb_active_batch++;
      continue;
    }

    __DMB();
    record = &uart_record_queue[uart_queue_tail];
    if (record->batch > uart_usb_active_batch)
    {
      uart_usb_active_batch++;
      continue;
    }
    if (record->batch < uart_usb_active_batch)
    {
      uart_queue_tail = UART_Queue_Next(uart_queue_tail);
      continue;
    }

    if (CDC_TransmitReady_FS() == 0U)
    {
      return;
    }

    line_length = UART_Format_Record(record, uart_usb_tx_line);
    if (CDC_Transmit_FS(uart_usb_tx_line, line_length) == USBD_OK)
    {
      uart_usb_tx_in_flight = 1U;
    }
    return;
  }
}

void MX_USART_DMA_Receive_Start(void)
{
  UART_Receive_Start_Once(&huart1, uart1_rx_buf, UART_RX_BUF_SIZE);
  UART_Receive_Start_Once(&huart2, uart2_rx_buf, UART_RX_BUF_SIZE);
  UART_Receive_Start_Once(&huart3, uart3_rx_buf, UART_RX_BUF_SIZE);
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  if (huart == &huart1)
  {
    UART_Record_Received(&huart1, uart1_rx_buf, Size);
    UART_Receive_Start_Once(&huart1, uart1_rx_buf, UART_RX_BUF_SIZE);
  }
  else if (huart == &huart2)
  {
    UART_Record_Received(&huart2, uart2_rx_buf, Size);
    UART_Receive_Start_Once(&huart2, uart2_rx_buf, UART_RX_BUF_SIZE);
  }
  else if (huart == &huart3)
  {
    UART_Record_Received(&huart3, uart3_rx_buf, Size);
    UART_Receive_Start_Once(&huart3, uart3_rx_buf, UART_RX_BUF_SIZE);
  }
}

/* USART1 init function */

void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 1000000;
  huart1.Init.WordLength = UART_WORDLENGTH_9B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_EVEN;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}
/* USART2 init function */

void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 1000000;
  huart2.Init.WordLength = UART_WORDLENGTH_9B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_EVEN;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}
/* USART3 init function */

void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 1000000;
  huart3.Init.WordLength = UART_WORDLENGTH_9B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_EVEN;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspInit 0 */

  /* USER CODE END USART1_MspInit 0 */
    /* USART1 clock enable */
    __HAL_RCC_USART1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* USART1 DMA Init */
    /* USART1_RX Init */
    hdma_usart1_rx.Instance = DMA1_Channel5;
    hdma_usart1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_usart1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart1_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart1_rx.Init.Mode = DMA_NORMAL;
    hdma_usart1_rx.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_usart1_rx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(uartHandle,hdmarx,hdma_usart1_rx);

    /* USART1_TX Init */
    hdma_usart1_tx.Instance = DMA1_Channel4;
    hdma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_usart1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart1_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart1_tx.Init.Mode = DMA_CIRCULAR;
    hdma_usart1_tx.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_usart1_tx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(uartHandle,hdmatx,hdma_usart1_tx);

    /* USART1 interrupt Init */
    HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
  /* USER CODE BEGIN USART1_MspInit 1 */

  /* USER CODE END USART1_MspInit 1 */
  }
  else if(uartHandle->Instance==USART2)
  {
  /* USER CODE BEGIN USART2_MspInit 0 */

  /* USER CODE END USART2_MspInit 0 */
    /* USART2 clock enable */
    __HAL_RCC_USART2_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* USART2 DMA Init */
    /* USART2_RX Init */
    hdma_usart2_rx.Instance = DMA1_Channel6;
    hdma_usart2_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_usart2_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart2_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart2_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart2_rx.Init.Mode = DMA_NORMAL;
    hdma_usart2_rx.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_usart2_rx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(uartHandle,hdmarx,hdma_usart2_rx);

    /* USART2_TX Init */
    hdma_usart2_tx.Instance = DMA1_Channel7;
    hdma_usart2_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_usart2_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart2_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart2_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart2_tx.Init.Mode = DMA_CIRCULAR;
    hdma_usart2_tx.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_usart2_tx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(uartHandle,hdmatx,hdma_usart2_tx);

    /* USART2 interrupt Init */
    HAL_NVIC_SetPriority(USART2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
  /* USER CODE BEGIN USART2_MspInit 1 */

  /* USER CODE END USART2_MspInit 1 */
  }
  else if(uartHandle->Instance==USART3)
  {
  /* USER CODE BEGIN USART3_MspInit 0 */

  /* USER CODE END USART3_MspInit 0 */
    /* USART3 clock enable */
    __HAL_RCC_USART3_CLK_ENABLE();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**USART3 GPIO Configuration
    PB10     ------> USART3_TX
    PB11     ------> USART3_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* USART3 DMA Init */
    /* USART3_RX Init */
    hdma_usart3_rx.Instance = DMA1_Channel3;
    hdma_usart3_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_usart3_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart3_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart3_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart3_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart3_rx.Init.Mode = DMA_NORMAL;
    hdma_usart3_rx.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_usart3_rx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(uartHandle,hdmarx,hdma_usart3_rx);

    /* USART3_TX Init */
    hdma_usart3_tx.Instance = DMA1_Channel2;
    hdma_usart3_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_usart3_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart3_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart3_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart3_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart3_tx.Init.Mode = DMA_CIRCULAR;
    hdma_usart3_tx.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_usart3_tx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(uartHandle,hdmatx,hdma_usart3_tx);

    /* USART3 interrupt Init */
    HAL_NVIC_SetPriority(USART3_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
  /* USER CODE BEGIN USART3_MspInit 1 */

  /* USER CODE END USART3_MspInit 1 */
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{

  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspDeInit 0 */

  /* USER CODE END USART1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART1_CLK_DISABLE();

    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9|GPIO_PIN_10);

    /* USART1 DMA DeInit */
    HAL_DMA_DeInit(uartHandle->hdmarx);
    HAL_DMA_DeInit(uartHandle->hdmatx);

    /* USART1 interrupt Deinit */
    HAL_NVIC_DisableIRQ(USART1_IRQn);
  /* USER CODE BEGIN USART1_MspDeInit 1 */

  /* USER CODE END USART1_MspDeInit 1 */
  }
  else if(uartHandle->Instance==USART2)
  {
  /* USER CODE BEGIN USART2_MspDeInit 0 */

  /* USER CODE END USART2_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART2_CLK_DISABLE();

    /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2|GPIO_PIN_3);

    /* USART2 DMA DeInit */
    HAL_DMA_DeInit(uartHandle->hdmarx);
    HAL_DMA_DeInit(uartHandle->hdmatx);

    /* USART2 interrupt Deinit */
    HAL_NVIC_DisableIRQ(USART2_IRQn);
  /* USER CODE BEGIN USART2_MspDeInit 1 */

  /* USER CODE END USART2_MspDeInit 1 */
  }
  else if(uartHandle->Instance==USART3)
  {
  /* USER CODE BEGIN USART3_MspDeInit 0 */

  /* USER CODE END USART3_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART3_CLK_DISABLE();

    /**USART3 GPIO Configuration
    PB10     ------> USART3_TX
    PB11     ------> USART3_RX
    */
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10|GPIO_PIN_11);

    /* USART3 DMA DeInit */
    HAL_DMA_DeInit(uartHandle->hdmarx);
    HAL_DMA_DeInit(uartHandle->hdmatx);

    /* USART3 interrupt Deinit */
    HAL_NVIC_DisableIRQ(USART3_IRQn);
  /* USER CODE BEGIN USART3_MspDeInit 1 */

  /* USER CODE END USART3_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
