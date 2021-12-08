/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define BUF_TX_LEN 1024
#define BUF_RX_LEN 512
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/
char Buf_TX[BUF_TX_LEN];
char Buf_RX[BUF_RX_LEN];
__IO int emptyTX=0;
__IO int busyTX=0;
__IO int emptyRX=0;
__IO int busyRX=0;


char bfr[261]; //ramka
volatile uint16_t pid=0;//wskaźnik ramki
volatile uint8_t statframe=0; //status ramki
char order[256]; //tablica polecenia

int czas=10;//wartosc domyslna dla FTIME
int wart=100;//wartosc domyslna dla FSIZE

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

uint8_t get_char(){
	uint8_t tmp;
	if(emptyRX != busyRX){
		tmp = Buf_RX[busyRX];
		busyRX++;
		if(busyRX >= BUF_RX_LEN){
			busyRX=0;
		}

		return tmp;
	}
	else{
		return 0;
	}
}
void fsend(char* format, ...){
	char tmp_rs[128];
	int i;
	__IO int pid;
	va_list arglist;
	va_start(arglist, format);
	vsprintf(tmp_rs, format, arglist);
	va_end(arglist);
	pid = emptyTX;
	for(i = 0; i < strlen(tmp_rs); i++){
		Buf_TX[pid] = tmp_rs[i];
		pid++;
		if(pid >= BUF_TX_LEN){
			pid = 0;
		}
	}
	__disable_irq();
	if((emptyTX == busyTX) && (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TXE) == SET)){
		emptyTX = pid;
		uint8_t tmp = Buf_TX[busyTX];
		busyTX++;
		if(busyTX >= BUF_TX_LEN)
			busyTX = 0;
		HAL_UART_Transmit_IT(&huart2, &tmp, 1);
	}
	else{
		emptyTX = pid;
	}
	__enable_irq();
}

void doner(char *ord){

	if (strcmp("FCHKL;", ord) == 0){
		fsend("Ilosc impulsow od zbocza narastajacego do opadajacego wynosi (liczba).\r\n");
	}
	else if(strcmp("FCHKH;", ord) == 0){
		fsend("Ilosc impulsow wyslanych w zadanym czasie wynosi (liczba).\r\n");
	}
	else if(strcmp("FSTART;", ord) == 0){
		fsend("Rozpoczeto wysylanie impulsow \r\n");
	}
	else if(strcmp("FSTAT;", ord) == 0){
		fsend("Wypelnienie (wartosc FSET) Czas (wartosc FTIME)\r\n");
	}
	else if(sscanf(ord, "FTIME%d;", &czas) == 1 || strcmp("FTIME;", ord) == 0){
		if(czas>=0 && czas<=120){
			fsend("„Ustawiono czas na %d sekund.\r\n",czas);
		}
		else{
			fsend("WRNUM\r\n");
		}
	}
	else if(sscanf(ord, "FSET%d;", &wart) == 1 || strcmp("FSET;", ord) == 0){
		if(wart>=0 && wart<=100){
			fsend("„Ustawiono wypelnienie na %d %.\r\n",wart);
		}
		else{
			fsend("WRNUM\r\n");
		}
	}
	else{
		fsend("WRCMD\r\n");
	}

}

void get_line(){

	if(statframe==0)
	{
		char temp = get_char();
		bfr[pid]=temp;

		if(temp == 0x05)
		{
			statframe=1;
		}
	}
	else if(statframe==1){
		pid++;
		if(pid > 261){
			pid=0;
			statframe=0;

		}
		char temp = get_char();
		bfr[pid]=temp;
		if(temp == 0x05){
			pid=0;
		}
		if(temp == 0x04){

			fsend(bfr);
			fsend("\r\n");
			int ordpid;
			int poi=1;
			for(int i=1;i<=pid;i++){
				if(bfr[i] == ';'){
					memset(&order[0],0,sizeof(order));
					ordpid=0;
					while(poi<=i){
						order[ordpid]=bfr[poi];
						ordpid++;
						poi++;
					}
					ordpid=i+1;
					doner(order);
				}
			}
			pid=0;
			statframe=0;


		}
	}
}





void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart == &huart2){
		if(emptyTX != busyTX){
			uint8_t tmp = Buf_TX[busyTX];
			busyTX++;
			if(busyTX >= BUF_TX_LEN){
				busyTX = 0;
			}
			HAL_UART_Transmit_IT(&huart2, &tmp, 1);
		}
	}
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{

	if (huart == &huart2){
		emptyRX++;
		if(emptyRX >= BUF_RX_LEN){
			emptyRX = 0;
		}
		HAL_UART_Receive_IT(&huart2,&Buf_RX[emptyRX], 1);
	}
}


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
  MX_USART2_UART_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  fsend("Hello user\r\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  HAL_UART_Receive_IT(&huart2,&Buf_RX[0], 1);

  while (1) {

	  if(busyRX!=emptyRX){
		  get_line();
	  }

    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */

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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
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
	while (1) {
	}
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
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
	 tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
