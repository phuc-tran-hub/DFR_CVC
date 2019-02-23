/*
 * cvc_logging.c
 *
 *  Created on: Feb 6, 2019
 *      Author: f002bc7
 */

/* Includes ------------------------------------------------------------------*/
#include "cvc_logging.h"

/* Function Prototypes ------------------------------------------------------------------*/
void logging_error();

/* Private Global Variables ------------------------------------------------------------------*/
static uint32_t buff[100U];
static FATFS SD_FatFs; /* File system object for User logical drive */
static FIL LogFile; /* File object */
static FIL CopyFile;
static char SDPath[4]; /* SD disk logical drive path */
static uint32_t wbytes; /* File write counts */
static uint32_t rbytes; /* File read counts */

static uint8_t n_outputs;	/* number of outputs to log */
static uint32_t n_writes = 0;	/* number of writes since power cycle */

CAN_input_t CAN_logging[] = {BAMO_BUS_VOLTAGE};		/* CAN inputs to log */
char * header = "TICKS,BAMO_BUS_VOLTAGE\n";	/* labels for file contents (separate by commas for csv */

/* Defines ------------------------------------------------------------------*/
#define WRITER


/* Functions ------------------------------------------------------------------*/

/**
 * @brief initialize logging functionality
 */
void logging_init()
{
	/* link/initialize SD driver */
	if (FATFS_LinkDriver(&SD_Driver, SDPath) == 0)
	{
		/* mount disk */
		if (f_mount(&SD_FatFs, (char const*) SDPath, 0) != FR_OK)
		{
			logging_error();
		}
	}
	else
	{
		logging_error();
	}

	/* get number of logging outputs */
	n_outputs = sizeof(CAN_logging) / sizeof(CAN_input_t);

}


void log_data(void)
{
	char *pbuff = (char *) buff;

	TickType_t ticks = xTaskGetTickCount();
	uint8_t i = 0;

	/* add header to buffer */
	if (n_writes == 0)
	{
		strcpy(pbuff+i, header);
		i+=strlen(pbuff+i);
	}

	/* add current ticks to buffer */
	utoa(ticks, pbuff+i, 10);
	i+=strlen(pbuff+i);
	*(pbuff+i) = ',';	// insert ASCII code for "," character
	i++;

	/* loop through logging outputs and add to buffer */
	// TODO: add critical sections around fopen->fclose
	for (uint8_t j = 0; j < n_outputs; j++)
	{
		xSemaphoreTake(CAN_Inputs_Vector_Mutex, portMAX_DELAY);
		utoa(CAN_inputs[CAN_logging[j]], pbuff+i, 10);
		xSemaphoreGive(CAN_Inputs_Vector_Mutex);

		i+=strlen(pbuff+i);

		if (j<n_outputs-1)
		{
			*(pbuff+i) = ',';	// separate values of same row by commas
		}
		else
		{
			*(pbuff+i) = '\n';		// insert new line at end of row
		}
		i++;
	}

	/* write data to file */
	if (f_open(&LogFile, "CAN_LOG.csv", FA_OPEN_APPEND | FA_WRITE) == FR_OK)
	{
		if(f_write(&LogFile, pbuff, i, (void *)&wbytes) != FR_OK)
		{
			f_close(&LogFile);
			logging_error();
		}
		f_close(&LogFile);

		/* increase number of writes */
		n_writes++;

		BSP_LED_On(LED_GREEN);
	}
	else
	{
		logging_error();
	}

//	taskEXIT_CRITICAL();

	if (n_writes >= 20)
	{
		BSP_LED_On(LED_BLUE);
		vTaskSuspend(NULL);
	}

}

void copy_file(void)
{
	char *pbuff = (char *) buff;

	/* create copy of existing file */
	if (f_open(&LogFile, "STM32.csv", FA_OPEN_EXISTING | FA_READ) == FR_OK && f_open(&CopyFile, "STM32_copy.csv", FA_OPEN_ALWAYS | FA_WRITE) == FR_OK)
	{
		FSIZE_t file_size = f_size(&LogFile);
		int buff_size = sizeof(buff);

		int n = file_size / buff_size;
		int r = file_size % buff_size;

		for (int j = 0; j <= n; j++)
		{

			if(f_read(&LogFile, pbuff, (j < n) ? buff_size : r, (void *)&rbytes) != FR_OK)
			{
				f_close(&LogFile);
				f_close(&CopyFile);
				logging_error();
			}
			if(f_write(&CopyFile, pbuff, rbytes, (void *)&wbytes) != FR_OK)
			{
				f_close(&LogFile);
				f_close(&CopyFile);
				logging_error();
			}
		}

		f_close(&LogFile);
		f_close(&CopyFile);

		BSP_LED_On(LED_BLUE);
	}
	else
	{
		logging_error();
	}
}


void logging_error()
{
	while(1);
}

