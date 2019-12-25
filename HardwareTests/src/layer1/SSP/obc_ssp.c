/*
 *  obc_ssp0.c
 *
 *  Created on: 07.10.2012
 *      Author: Andi
 *
 *  Copied over from pegasus flight software on 2019-12-14
 */

#include <stdio.h>
#include <string.h>

#include <chip.h>

#include "obc_ssp.h"

ssp_jobs_t ssp_jobs[2];	
ssp_status_t ssp_status[2];

#define SSP0_SCK_PIN 20 //ok
#define SSP0_SCK_PORT 1 //ok
#define SSP0_MISO_PIN 23 //ok
#define SSP0_MISO_PORT 1 //ok
#define SSP0_MOSI_PIN 24 //ok
#define SSP0_MOSI_PORT 1 //ok
//#define SSP0_FUNCTION_NUMBER

#define FLASH2_CS1_PIN 12 //ok
#define FLASH2_CS1_PORT 2 //ok
#define FLASH2_CS2_PIN 11 //ok
#define FLASH2_CS2_PORT 2 //ok

#define SSP1_SCK_PIN 7
#define SSP1_SCK_PORT 0
#define SSP1_MISO_PIN 8
#define SSP1_MISO_PORT 0
#define SSP1_MOSI_PIN 9
#define SSP1_MOSI_PORT 0
//#define SSP1_FUNCTION_NUMBER 2

#define FLASH1_CS1_PIN 28
#define FLASH1_CS1_PORT 4
#define FLASH1_CS2_PIN 2
#define FLASH1_CS2_PORT 2

// from RTOS
#define configMAX_LIBRARY_INTERRUPT_PRIORITY    ( 5 )
#define SSP1_INTERRUPT_PRIORITY         (configMAX_LIBRARY_INTERRUPT_PRIORITY + 3)  /* SSP1 (Flash, MPU) */
#define SSP0_INTERRUPT_PRIORITY         (SSP1_INTERRUPT_PRIORITY + 1)   /* SSP0 (Flash) - should be lower than SSP1 */

//typedef long BaseType_t;
//#define pdFALSE			( ( BaseType_t ) 0 )

volatile bool flash1_busy;		// temp 'ersatz' für semaphor
volatile bool flash2_busy;		// temp 'ersatz' für semaphor

// Prototypes
void SSP01_IRQHandler(LPC_SSP_T *device, uint8_t   deviceNr);

// Common init routine used for both SSP buses
void ssp_init(LPC_SSP_T *device, uint8_t busNr, IRQn_Type irq, uint32_t irqPrio ) {
	/* SSP Init */
	uint32_t helper;

	/* Prevent compiler warning */
	(void) helper;

	Chip_SSP_Set_Mode(device, SSP_MODE_MASTER);
	Chip_SSP_SetFormat(device, SSP_BITS_8, SSP_FRAMEFORMAT_SPI, SSP_CLOCK_CPHA0_CPOL0);
	Chip_SSP_SetBitRate(device, 4000000);		// -> TODO ergibt 400 kHz Clock rate !???

	Chip_SSP_DisableLoopBack(device);
	Chip_SSP_Enable(device);

	while ((device->SR & SSP_STAT_RNE) != 0) /* Flush RX FIFO */
	{
		helper = device->DR;
	}

	//SSP_IntConfig(LPC_SSP0, SSP_INTCFG_RT, ENABLE);
	//SSP_IntConfig(LPC_SSP0, SSP_INTCFG_ROR, ENABLE);
	//SSP_IntConfig(LPC_SSP0, SSP_INTCFG_RX, ENABLE);
	// no function found for this one !?
	device->IMSC |= SSP_RTIM;
	device->IMSC |= SSP_RORIM;
	device->IMSC |= SSP_RXIM;

	/* Clear interrupt flags */
	device->ICR = SSP_RORIM;
	device->ICR = SSP_RTIM;

	/* Reset buffers to default values */
	ssp_jobs[busNr].current_job = 0;
	ssp_jobs[busNr].jobs_pending = 0;
	ssp_jobs[busNr].last_job_added = 0;

	NVIC_SetPriority(irq, irqPrio);
	NVIC_EnableIRQ (irq);

	ssp_status[busNr].ssp_error_counter = 0;
	ssp_status[busNr].ssp_initialized = 1;

}

// Module Init
void ssp01_init(void)
{
	ssp_status[0].ssp_initialized = 0;
	ssp_status[1].ssp_initialized = 0;

	/* --- SSP0 pins --- */
	Chip_IOCON_PinMuxSet(LPC_IOCON, SSP0_SCK_PORT, SSP0_SCK_PIN, IOCON_FUNC3 | IOCON_MODE_INACT);
	Chip_IOCON_DisableOD(LPC_IOCON, SSP0_SCK_PORT, SSP0_SCK_PIN);

	Chip_IOCON_PinMuxSet(LPC_IOCON, SSP0_MISO_PORT, SSP0_MISO_PIN, IOCON_FUNC3 | IOCON_MODE_INACT);
	Chip_IOCON_DisableOD(LPC_IOCON, SSP0_MISO_PORT, SSP0_MISO_PIN);

	Chip_IOCON_PinMuxSet(LPC_IOCON, SSP0_MOSI_PORT, SSP0_MOSI_PIN, IOCON_FUNC3 | IOCON_MODE_INACT);
	Chip_IOCON_DisableOD(LPC_IOCON, SSP0_MOSI_PORT, SSP0_MOSI_PIN);

	/* --- Chip selects --- */
	Chip_IOCON_PinMuxSet(LPC_IOCON, FLASH2_CS1_PORT, FLASH2_CS1_PIN, IOCON_FUNC0 | IOCON_MODE_INACT);
	Chip_IOCON_DisableOD(LPC_IOCON, FLASH2_CS1_PORT, FLASH2_CS1_PIN);
	Chip_GPIO_SetPinDIROutput(LPC_GPIO, FLASH2_CS1_PORT, FLASH2_CS1_PIN);
	Chip_GPIO_SetPinState(LPC_GPIO, FLASH2_CS1_PORT, FLASH2_CS1_PIN, true);

	Chip_IOCON_PinMuxSet(LPC_IOCON, FLASH2_CS2_PORT, FLASH2_CS2_PIN, IOCON_FUNC0 | IOCON_MODE_INACT);
	Chip_IOCON_DisableOD(LPC_IOCON, FLASH2_CS2_PORT, FLASH2_CS2_PIN);
	Chip_GPIO_SetPinDIROutput(LPC_GPIO, FLASH2_CS2_PORT, FLASH2_CS2_PIN);
	Chip_GPIO_SetPinState(LPC_GPIO, FLASH2_CS2_PORT, FLASH2_CS2_PIN, true);

	/* --- SSP1 pins --- */
	Chip_IOCON_PinMuxSet(LPC_IOCON, SSP1_SCK_PORT, SSP1_SCK_PIN, IOCON_FUNC2 | IOCON_MODE_INACT);
	Chip_IOCON_DisableOD(LPC_IOCON, SSP1_SCK_PORT, SSP1_SCK_PIN);

	Chip_IOCON_PinMuxSet(LPC_IOCON, SSP1_MISO_PORT, SSP1_MISO_PIN, IOCON_FUNC2 | IOCON_MODE_INACT);
	Chip_IOCON_DisableOD(LPC_IOCON, SSP1_MISO_PORT, SSP1_MISO_PIN);

	Chip_IOCON_PinMuxSet(LPC_IOCON, SSP1_MOSI_PORT, SSP1_MOSI_PIN, IOCON_FUNC2 | IOCON_MODE_INACT);
	Chip_IOCON_DisableOD(LPC_IOCON, SSP1_MOSI_PORT, SSP1_MOSI_PIN);

	/* --- Chip selects --- */
	Chip_IOCON_PinMuxSet(LPC_IOCON, FLASH1_CS1_PORT, FLASH1_CS1_PIN, IOCON_FUNC0 | IOCON_MODE_INACT);
	Chip_IOCON_DisableOD(LPC_IOCON, FLASH1_CS1_PORT, FLASH1_CS1_PIN);
	Chip_GPIO_SetPinDIROutput(LPC_GPIO, FLASH1_CS1_PORT, FLASH1_CS1_PIN);
	Chip_GPIO_SetPinState(LPC_GPIO, FLASH1_CS1_PORT, FLASH1_CS1_PIN, true);

	Chip_IOCON_PinMuxSet(LPC_IOCON, FLASH1_CS2_PORT, FLASH1_CS2_PIN, IOCON_FUNC0 | IOCON_MODE_INACT);
	Chip_IOCON_DisableOD(LPC_IOCON, FLASH1_CS2_PORT, FLASH1_CS2_PIN);
	Chip_GPIO_SetPinDIROutput(LPC_GPIO, FLASH1_CS2_PORT, FLASH1_CS2_PIN);
	Chip_GPIO_SetPinState(LPC_GPIO, FLASH1_CS2_PORT, FLASH1_CS2_PIN, true);

	Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_SSP0);
	Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_SSP1);

	ssp_init(LPC_SSP0, 0, SSP0_IRQn ,SSP0_INTERRUPT_PRIORITY);
	ssp_init(LPC_SSP1, 1, SSP1_IRQn ,SSP1_INTERRUPT_PRIORITY);

	return;
}

void SSP1_IRQHandler(void)
{
	SSP01_IRQHandler(LPC_SSP1, 1);
}


void SSP0_IRQHandler(void)
{
	SSP01_IRQHandler(LPC_SSP0, 0);
}

void SSP01_IRQHandler(LPC_SSP_T *device, uint8_t   deviceNr) {

	uint32_t int_src = device->RIS; /* Get interrupt source */
	volatile uint32_t helper;

	if (int_src == SSP_TXIM)
	{
		/* TX buffer half empty interrupt is not used but may occur */
		return;
	}

	if ((int_src & SSP_RORIM))
	{
		device->ICR = SSP_RORIM;
		ssp_status[deviceNr].ssp_error_counter++;
		ssp_status[deviceNr].ssp_interrupt_ror = 1;
		return;
	}

	if ((int_src & SSP_RTIM))	// Clear receive timeout
	{
		device->ICR = SSP_RTIM;
	}

	if (ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].dir)
	{
		/* --- TX ------------------------------------------------------------------------------------------------------------------------ */

		// Dump RX
		while ((device->SR & SSP_STAT_RNE) != 0) /* Flush RX FIFO */
		{
			helper = device->DR;
		}

		/* Fill TX FIFO */

		if ((ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_to_send - ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent) > 7)
		{

			helper = ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent + 7;
		}
		else
		{
			helper = ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_to_send;
		}

		//while (((LPC_SSP0->SR & SSP_STAT_TXFIFO_NOTFULL)) && (ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent < helper))
		while (((device->SR & SSP_STAT_TNF)) && (ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent < helper))
		{
			device->DR = ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].array_to_send[ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent];
			ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent++;
		}

		//if (LPC_SSP0->SR & SSP_SR_BSY)
		if (device->SR & SSP_STAT_BSY)
			return;

		if ((ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent == ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_to_send))
		{
			/* TX done */
			/* Check if job includes SSP read */
			if (ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_to_read > 0)
			{
				/* RX init */
				ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].dir = 0; /* set to read */
				while ((device->SR & SSP_STAT_RNE) != 0) /* Flush RX FIFO */
				{
					helper = device->DR;
				}

				ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent = 0;

				if ((ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_to_read - ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent) > 7)
				{

					helper = ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent + 7;
				}
				else
				{
					helper = ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_to_read;
				}

				while (((device->SR & SSP_STAT_TNF)) && (ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent < helper))
				{
					device->DR = 0xFF;
					ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent++;
				}

				helper = 0;
				/* Wait for interrupt*/
			}
			else
			{
				/* transfer done */
				/* release chip select and return */
				helper = 0;
				while ((device->SR & SSP_STAT_BSY) && (helper < 100000))
				{
					/* Wait for SSP to finish transmission */
					helper++;
				}

				/* Unselect device */
				switch (ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].device)
				{
					case SSPx_DEV_FLASH2_1:
						Chip_GPIO_SetPortValue(LPC_GPIO, FLASH2_CS1_PORT, 1 << FLASH2_CS1_PIN);
						flash2_busy = false;
						break;

					case SSPx_DEV_FLASH2_2:
						Chip_GPIO_SetPortValue(LPC_GPIO, FLASH2_CS2_PORT, 1 << FLASH2_CS2_PIN);
						flash2_busy = false;
						break;

					case SSPx_DEV_FLASH1_1:
						Chip_GPIO_SetPortValue(LPC_GPIO, FLASH1_CS1_PORT, 1 << FLASH1_CS1_PIN);
						flash1_busy = false;
						break;

					case SSPx_DEV_FLASH1_2:
						Chip_GPIO_SetPortValue(LPC_GPIO, FLASH1_CS2_PORT, 1 << FLASH1_CS2_PIN);
						flash1_busy = false;
						break;

					default: /* Device does not exist */
						/* Release all devices */
						ssp_status[deviceNr].ssp_error_counter++;
						ssp_status[deviceNr].ssp_interrupt_unknown_device = 1;
						Chip_GPIO_SetPortValue(LPC_GPIO,FLASH2_CS2_PORT, 1 << FLASH2_CS2_PIN);
						Chip_GPIO_SetPortValue(LPC_GPIO,FLASH2_CS1_PORT, 1 << FLASH2_CS1_PIN);
						Chip_GPIO_SetPortValue(LPC_GPIO, FLASH1_CS1_PORT, 1 << FLASH1_CS1_PIN);
						Chip_GPIO_SetPortValue(LPC_GPIO, FLASH1_CS2_PORT, 1 << FLASH1_CS2_PIN);
						break;
				}

				ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].status = SSP_JOB_STATE_DONE;
			}
		}

	}
	else
	{
		/* --- RX ------------------------------------------------------------------------------------------------------------------------ */

		/* Read from RX FIFO */

		//while ((LPC_SSP0->SR & SSP_STAT_RXFIFO_NOTEMPTY)
		while ((device->SR & SSP_STAT_RNE)
		        && (ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_read < ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_to_read))
		{
			ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].array_to_read[ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_read] = device->DR;
			ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_read++;
		}

		if (ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_read == ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_to_read)
		{
			/* All bytes read */

			helper = 0;
			while ((device->SR & SSP_STAT_BSY) && (helper < 100000))
			{
				/* Wait for SSP to finish transmission */
				helper++;
			}

			switch (ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].device)
			/* Unselect device */
			{

				case SSPx_DEV_FLASH2_1:
					Chip_GPIO_SetValue(LPC_GPIO,FLASH2_CS1_PORT, 1 << FLASH2_CS1_PIN);
					flash2_busy = false;
					break;

				case SSPx_DEV_FLASH2_2:
					Chip_GPIO_SetValue(LPC_GPIO,FLASH2_CS2_PORT, 1 << FLASH2_CS2_PIN);
					flash2_busy = false;
					break;

				case SSPx_DEV_FLASH1_1:
					Chip_GPIO_SetValue(LPC_GPIO,FLASH1_CS1_PORT, 1 << FLASH1_CS1_PIN);
					flash1_busy = false;
					break;

				case SSPx_DEV_FLASH1_2:
					Chip_GPIO_SetValue(LPC_GPIO,FLASH1_CS2_PORT, 1 << FLASH1_CS2_PIN);
					flash1_busy = false;
					break;

				default: /* Device does not exist */
					/* Release all devices */
					ssp_status[deviceNr].ssp_interrupt_unknown_device = 1;
					Chip_GPIO_SetValue(LPC_GPIO,FLASH2_CS2_PORT, 1 << FLASH2_CS2_PIN);
					Chip_GPIO_SetValue(LPC_GPIO,FLASH2_CS1_PORT, 1 << FLASH2_CS1_PIN);
					Chip_GPIO_SetValue(LPC_GPIO,FLASH1_CS1_PORT, 1 << FLASH1_CS1_PIN);
					Chip_GPIO_SetValue(LPC_GPIO,FLASH1_CS2_PORT, 1 << FLASH1_CS2_PIN);
					break;
			}

			ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].status = SSP_JOB_STATE_DONE;
		}
		else
		{
			/* not all bytes read - send dummy data again */

			if ((ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_to_read - ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent) > 7)
			{

				helper = ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent + 7;
			}
			else
			{
				helper = ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_to_read;
			}

			while ((device->SR & SSP_STAT_TNF) && (ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent < helper))
			{
				device->DR = 0xFF;
				ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent++;
			}
		}
	}

	if (ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].status == SSP_JOB_STATE_DONE)
	{
		/* Job is done, increment to next job and execute if pending */

		ssp_jobs[deviceNr].current_job++;
		ssp_jobs[deviceNr].jobs_pending--;

		if (ssp_jobs[deviceNr].current_job == SPI_MAX_JOBS)
		{
			ssp_jobs[deviceNr].current_job = 0;
		}

		while ((device->SR & SSP_STAT_RNE) != 0) /* Flush RX FIFO */
		{
			helper = device->DR;
		}

		/* Check if jobs are pending */
		if (ssp_jobs[deviceNr].jobs_pending > 0)
		{
			switch (ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].device)
			/* Select device */
			{
				case SSPx_DEV_FLASH2_1:
					Chip_GPIO_ClearValue(LPC_GPIO, FLASH2_CS1_PORT, 1 << FLASH2_CS1_PIN);
					break;

				case SSPx_DEV_FLASH2_2:
					Chip_GPIO_ClearValue(LPC_GPIO,FLASH2_CS2_PORT, 1 << FLASH2_CS2_PIN);
					break;

				case SSPx_DEV_FLASH1_1:
					Chip_GPIO_ClearValue(LPC_GPIO, FLASH1_CS1_PORT, 1 << FLASH1_CS1_PIN);
					break;

				case SSPx_DEV_FLASH1_2:
					Chip_GPIO_ClearValue(LPC_GPIO,FLASH1_CS2_PORT, 1 << FLASH1_CS2_PIN);
					break;

				default: /* Device does not exist */
					ssp_status[deviceNr].ssp_error_counter++;
					Chip_GPIO_SetValue(LPC_GPIO, FLASH2_CS1_PORT, 1 << FLASH2_CS1_PIN);		// TODO ??? hier war set obwohl obemn clears sind !? -----
					Chip_GPIO_SetValue(LPC_GPIO, FLASH2_CS2_PORT, 1 << FLASH2_CS2_PIN);
					Chip_GPIO_SetValue(LPC_GPIO, FLASH1_CS1_PORT, 1 << FLASH1_CS1_PIN);
					Chip_GPIO_SetValue(LPC_GPIO, FLASH1_CS2_PORT, 1 << FLASH1_CS2_PIN);

					/* Set error description */
					ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].status = SSP_JOB_STATE_DEVICE_ERROR;

					/* Increment to next job */
					ssp_jobs[deviceNr].current_job++;
					ssp_jobs[deviceNr].jobs_pending--;

					if (ssp_jobs[deviceNr].current_job == SPI_MAX_JOBS)
					{
						ssp_jobs[deviceNr].current_job = 0;
					}

					return;

					break;

			}

			ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].status = SSP_JOB_STATE_ACTIVE;

			/* Fill FIFO */
			if (ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].dir)
			{
				/* TX (+RX) */
				if ((ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_to_send - ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent) > 7)
				{
					helper = ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent + 7;
				}
				else
				{
					helper = ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_to_send;
				}

				while (((device->SR & SSP_STAT_TNF)) && (ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent < helper))
				{
					device->DR = ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].array_to_send[ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent];
					ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent++;
				}
			}
			else
			{
				/* RX only - send dummy data for clock output */

				ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent = 0; /* Use unused bytes_sent for counting sent dummy data bytes */

				if ((ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_to_read - ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent) > 7)
				{

					helper = ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent + 7;
				}
				else
				{
					helper = ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_to_read;
				}

				while (((device->SR & SSP_STAT_TNF)) && (ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent < helper))
				{
					device->DR = 0xFF;
					ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent++;
				}
			}
		}
	}

	//portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
	return;

	/* Max. execution time: 2863 cycles */
	/* Average execution time: 626 cycles */

}

uint32_t ssp_add_job(uint8_t deviceNr, uint8_t sensor, uint8_t *array_to_send, uint16_t bytes_to_send, uint8_t *array_to_store, uint16_t bytes_to_read,
        uint8_t **job_status)
{
	uint32_t helper;
	uint8_t position;

	LPC_SSP_T *device;
	if (deviceNr == 0) {
		device = LPC_SSP0;
	} else if (deviceNr == 1) {
		device = LPC_SSP1;
	} else {
		return SSP_WRONG_BUSNR;
	}

	if (ssp_status[deviceNr].ssp_initialized == 0)
	{
		/* SSP is not initialized - return */
		return SSP_JOB_NOT_INITIALIZED;
	}

	if (ssp_jobs[deviceNr].jobs_pending >= SPI_MAX_JOBS)
	{
		/* Maximum amount of jobs stored, job can't be added! */
		/* This is possibly caused by a locked interrupt -> remove all jobs and re-init SSP */
		//taskENTER_CRITICAL();
		ssp_status[deviceNr].ssp_error_counter++;
		ssp_status[deviceNr].ssp_buffer_overflow = 1;
		ssp_jobs[deviceNr].jobs_pending = 0; /* Delete jobs */
		ssp01_init(); /* Reinit SSP   make re-init per SSP nr possible here !?*/
		//taskEXIT_CRITICAL();
		return SSP_JOB_BUFFER_OVERFLOW;
	}

	// taskENTER_CRITICAL();		TODO: need for real multithreading.
	{
		position = (ssp_jobs[deviceNr].current_job + ssp_jobs[deviceNr].jobs_pending) % SPI_MAX_JOBS;

		ssp_jobs[deviceNr].job[position].array_to_send = array_to_send;
		ssp_jobs[deviceNr].job[position].bytes_to_send = bytes_to_send;
		ssp_jobs[deviceNr].job[position].bytes_sent = 0;
		ssp_jobs[deviceNr].job[position].array_to_read = array_to_store;
		ssp_jobs[deviceNr].job[position].bytes_to_read = bytes_to_read;
		ssp_jobs[deviceNr].job[position].bytes_read = 0;
		ssp_jobs[deviceNr].job[position].device = sensor;
		ssp_jobs[deviceNr].job[position].status = SSP_JOB_STATE_PENDING;

		if (bytes_to_send > 0)
		{
			/* Job contains transfer and read eventually */
			ssp_jobs[deviceNr].job[position].dir = 1;
		}
		else
		{
			/* Job contains readout only - transfer part is skipped */
			ssp_jobs[deviceNr].job[position].dir = 0;
		}

		/* Check if SPI in use */
		if (ssp_jobs[deviceNr].jobs_pending == 0)
		{ /* Check if jobs pending */
			switch (ssp_jobs[deviceNr].job[position].device)
			/* Select device */
			{
				case SSPx_DEV_FLASH2_1:
					Chip_GPIO_ClearValue(LPC_GPIO, FLASH2_CS1_PORT, 1 << FLASH2_CS1_PIN);
					break;

				case SSPx_DEV_FLASH2_2:
					Chip_GPIO_ClearValue(LPC_GPIO, FLASH2_CS2_PORT, 1 << FLASH2_CS2_PIN);
					break;

				case SSPx_DEV_FLASH1_1:
					Chip_GPIO_ClearValue(LPC_GPIO, FLASH1_CS1_PORT, 1 << FLASH1_CS1_PIN);
					break;

				case SSPx_DEV_FLASH1_2:
					Chip_GPIO_ClearValue(LPC_GPIO, FLASH1_CS2_PORT, 1 << FLASH1_CS2_PIN);
					break;


				default: /* Device does not exist */
					/* Unselect all device */
					Chip_GPIO_SetValue(LPC_GPIO, FLASH2_CS1_PORT, 1 << FLASH2_CS1_PIN);
					Chip_GPIO_SetValue(LPC_GPIO, FLASH2_CS2_PORT, 1 << FLASH2_CS2_PIN);
					Chip_GPIO_SetValue(LPC_GPIO, FLASH1_CS1_PORT, 1 << FLASH1_CS1_PIN);
					Chip_GPIO_SetValue(LPC_GPIO, FLASH1_CS2_PORT, 1 << FLASH1_CS2_PIN);

					ssp_status[deviceNr].ssp_error_counter++;

					/* Set error description */
					ssp_jobs[deviceNr].job[position].status = SSP_JOB_STATE_DEVICE_ERROR;

					/* Increment to next job */
					ssp_jobs[deviceNr].current_job++;
					ssp_jobs[deviceNr].jobs_pending--;

					if (ssp_jobs[deviceNr].current_job == SPI_MAX_JOBS)
					{
						ssp_jobs[deviceNr].current_job = 0;
					}

					/* Return error */
					return SSP_JOB_ERROR;
					break;

			}

			ssp_jobs[deviceNr].job[position].status = SSP_JOB_STATE_ACTIVE;

			while ((device->SR & SSP_STAT_RNE) != 0) /* Flush RX FIFO */
			{
				helper = device->DR;
			}

			/* Fill FIFO */

			if (ssp_jobs[deviceNr].job[position].dir)
			{
				/* TX (+RX) */

				if ((ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_to_send - ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent) > 7)
				{

					helper = ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent + 7;
				}
				else
				{
					helper = ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_to_send;
				}

				while (((device->SR & SSP_STAT_TNF)) && (ssp_jobs[deviceNr].job[position].bytes_sent < helper))
				{
					device->DR = ssp_jobs[deviceNr].job[position].array_to_send[ssp_jobs[deviceNr].job[position].bytes_sent];
					ssp_jobs[deviceNr].job[position].bytes_sent++;
				}
			}
			else
			{
				/* RX only - send dummy data for clock output */
				/* Use unused bytes_sent for counting sent dummy data bytes */

				if ((ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_to_read - ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent) > 7)
				{

					helper = ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_sent + 7;
				}
				else
				{
					helper = ssp_jobs[deviceNr].job[ssp_jobs[deviceNr].current_job].bytes_to_read;
				}

				while (((device->SR & SSP_STAT_TNF)) && (ssp_jobs[deviceNr].job[position].bytes_sent < helper))
				{
					device->DR = 0xFF;
					ssp_jobs[deviceNr].job[position].bytes_sent++;

				}
			}
		}

		ssp_jobs[deviceNr].jobs_pending++;
	}

	/* Set pointer to job status if necessary */
	if (job_status != NULL)
	{
		*job_status = (uint8_t *) &(ssp_jobs[deviceNr].job[position].status);
	}

	// taskEXIT_CRITICAL();	TODO needed for real multithreading
	return SSP_JOB_ADDED; /* Job added successfully */
}

