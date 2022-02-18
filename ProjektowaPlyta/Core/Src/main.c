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
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "nokia5110_LCD.h"
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
uint8_t Buf_TX[BUF_TX_LEN];
uint8_t Buf_RX[BUF_RX_LEN];
__IO int emptyTX=0;
__IO int busyTX=0;
__IO int emptyRX=0;
__IO int busyRX=0;


char bfr[261]; //ramka
volatile uint16_t fid=0;//wskaźnik ramki
int fstate = 0;
enum state{listen=1, notlisten=0};
char order[256]; //tablica polecenia
int czas=10;//wartosc domyslna dla FTIME
int wart=65535;//wartosc domyslna dla FFILL
int czest=10;//wartos domyslna dla FSET
char ENQ = 0x05;
char EOT = 0x04;


uint32_t Difference=0;
uint32_t pwmData[4];
uint32_t seconds_passed = 0;
uint32_t period=0;
int licznik=0;
int width = 1;
int riseCaptured = 0;
int fallCaptured = 0;
uint32_t riseData[1];
uint32_t fallData[1];
int isMeasured = 0;
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
int checkSum(char *buffer)
{
	int suma = 0;
	int dlugosc = strlen(buffer);
	int i;
	for(i = 0; i<dlugosc-1; i++){
			suma=suma+buffer[i];
		}
	return suma%256;
}
void fmessage(char msg[], char dst[]){
	int ctrlSumMsg = checkSum(msg);
	sprintf(dst, "%c%s;%02X%c\r\n",EOT,msg,ctrlSumMsg,ENQ);
}
void fsend(char* format, ...){
	char tmp_rs[256];
	int i;
	__IO int pid;
	va_list arglist;
	va_start(arglist, format);
	vsprintf(tmp_rs, format, arglist);
	va_end(arglist);
	char fmsg[261]={0};
	fmessage(tmp_rs, fmsg);
	pid = emptyTX;
	for(i = 0; i < strlen(fmsg); i++){
		Buf_TX[pid] = fmsg[i];
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

void wypelnienie(int wartosc, uint32_t period){


}

void start(){
			period=(64000000/(czest*1000))-1;
			wypelnienie(wart,period);
			TIM1->CCR1=wart;
			htim1.Init.Period = period;
			HAL_TIM_Base_Start_IT(&htim3);
			HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_1,(uint32_t *)pwmData, 4);
			HAL_TIM_IC_Start_DMA(&htim2, TIM_CHANNEL_1,riseData,1);
			HAL_TIM_IC_Start_DMA(&htim2, TIM_CHANNEL_2,fallData,1);
}
void doner(char *ord){

	if (strcmp("FCHKL;", ord) == 0){

		fsend("Ilosc impulsow od zbocza narastajacego do opadajacego wynosi %d",Difference);

	}
	else if(strcmp("FCHKH;", ord) == 0){

		fsend("Ilosc impulsow wyslanych w zadanym czasie wynosi %d",riseCaptured/4);

	}
	else if(strcmp("FSTART;", ord) == 0){
		fsend("Rozpoczeto wysylanie impulsow");
		seconds_passed=0;
		Difference = 0;
		riseCaptured = 0;
		fallCaptured = 0;
		start();
	}
	else if(strcmp("FSTAT;", ord) == 0){
		fsend("Wypelnienie %d Czas %d Czestotliwosc %d",wart,czas,czest);
	}
	else if(sscanf(ord, "FTIME%d;", &czas) == 1 || strcmp("FTIME;", ord) == 0){
		if(czas>=0 && czas<=20){
			fsend("Ustawiono czas na %d sekund",czas);
		}
		else{
			fsend("WRNUM");
		}
	}
	else if(sscanf(ord, "FFILL%d;", &wart) == 1 || strcmp("FFIL;", ord) == 0){
		if(wart>=0 && wart<= 4294967295){
			fsend("Ustawiono wypelnienie na %d",wart);
		}
		else{
			fsend("WRNUM");
		}
	}
	else if(sscanf(ord, "FSET%d;", &czest) == 1 || strcmp("FSET;", ord) == 0){
		if(czest>=1 && czest<=250){
					fsend("Ustawiono czestotliwosc na %d kHz",czest);
				}
				else{
					fsend("WRNUM");
				}
	}
	else{
		fsend("WRCMD");
	}

}

int hexVal(char *buffer){
	int suma = 0;
	int dlugosc = strlen(buffer);
	int i;
	int miejsca = 1;
	for(i = dlugosc-1; i>=0; i--){
		if(buffer[i]>='0' && buffer[i]<='9'){
			suma = suma +(buffer[i] - 48)*miejsca;
		}else if(buffer[i]>='A' && buffer[i]<='F'){
			suma = suma +(buffer[i] - 55)*miejsca;
		}
		miejsca = miejsca*16;
	}
	return suma;
}

void get_analyze(){
	char temp = get_char();//Pobranie znaku do zmiennej

	if(temp==0x05){
		fstate=listen;
		fid = 0;
		memset(&bfr[0],0, sizeof(bfr));
		bfr[fid] = temp;
		fid = fid+1;
	}
	else if(temp == 0x04 && fstate == listen){
		fstate = notlisten;
		bfr[fid]= temp;
		fid++;
		char ctrlSumFrame[3]={bfr[fid-3],
				bfr[fid-2], '\0'};
		int ctrlSumUser = hexVal(ctrlSumFrame);
		bfr[fid-3]='\0';
		memmove(&bfr[0],&bfr[1],strlen(bfr));
		int ctrlSumProgram = checkSum(bfr);
		if(ctrlSumProgram == ctrlSumUser){
			int frm_id = 0;
			int ord_id;
			int i;
			for(i = 0; i< fid; i++){
				if(bfr[i]==';'){
					memset(&order[0],0,sizeof(order));
					ord_id = 0;
					while(frm_id <= i){
						order[ord_id] = bfr[frm_id];
						frm_id++;
						ord_id++;
					}
					frm_id = i + 1;
					doner(order);
				}
			}
		}else{
			fsend("WRCHS%02X",ctrlSumProgram);
		}
	}else if(fstate == listen){
		if(!(temp > 0x21 && temp < 0x7E)){
			fsend("WRFRM");
			fstate = notlisten;
		}else{
			bfr[fid] = temp;
			fid = fid + 1;
			if(fid > 261){
				fsend("WRFRL");
				fstate = notlisten;
			}
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
  LCD_setRST(RST_GPIO_Port, RST_Pin);
  LCD_setCE(CE_GPIO_Port, CE_Pin);
  LCD_setDC(DC_GPIO_Port, DC_Pin);
      LCD_setDIN(DIN_GPIO_Port, DIN_Pin);
      LCD_setCLK(CLK_GPIO_Port, CLK_Pin);
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_TIM4_Init();
  MX_DMA_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  LCD_init();
  fsend("Czestotliwosciomierz");

  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1); //PWM dla ekranu
  __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, 100);

  	  LCD_print("Miernik", 0, 0);
  	  LCD_print("Czestotliwosci", 0, 1);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  HAL_UART_Receive_IT(&huart2,&Buf_RX[0], 1);

  while (1) {

	  if(busyRX!=emptyRX){
		  get_analyze();
	  }

	  if (isMeasured==1)
	  	 	  {
	  	 		  TIM2->CNT = 0;

	  	 		  HAL_TIM_IC_Start_DMA(&htim2, TIM_CHANNEL_1, riseData, 1);

	  	 		  HAL_TIM_IC_Start_DMA(&htim2, TIM_CHANNEL_2, fallData, 1);

	  	 		  isMeasured = 0;
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL8;
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
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
		if (htim->Instance == TIM2 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
		{
			riseCaptured++;

		}

		if (htim->Instance == TIM2 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2)
		{

			fallCaptured++;
			if(licznik>3){
				licznik = 0;
			}
			TIM1->CCR1=pwmData[licznik];
			licznik++;

		}
		if ((riseCaptured)&&(fallCaptured)){

			isMeasured = 1;
			width++;
			if(width<=4){
				if(fallData[0]>riseData[0]){
					Difference = Difference + (fallData[0]-riseData[0]);
				}else{
					Difference = Difference + (riseData[0]-fallData[0]);
				}
			}


		}
}


void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){

	if(htim->Instance == TIM3){
		seconds_passed += 1;
		if(seconds_passed>=czas)
		{
			HAL_TIM_PWM_Stop_DMA(&htim1, TIM_CHANNEL_1);
			HAL_TIM_IC_Stop_DMA(&htim2, TIM_CHANNEL_1);
			HAL_TIM_IC_Stop_DMA(&htim2, TIM_CHANNEL_2);
			HAL_TIM_Base_Stop_IT(&htim3);

			fsend("Przesylanie zakonczone");
		}
	}

}



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
