/*
 * mram.c
 *
 *  Created on: 27.12.2019
 */

#include <stdio.h>
#include <stdlib.h>
#include <chip.h>

#include "../../layer1/SSP/obc_ssp.h"
#include "../cli/cli.h"

#include "mram.h"

#define MRAM_CS_PORT	 0
#define MRAM_CS_PIN		22

typedef enum mram_status_e {
	MRAM_STAT_NOT_INITIALIZED,
	MRAM_STAT_IDLE,
	MRAM_STAT_RX_INPROGRESS,
	MRAM_STAT_WREN_SET,
	MRAM_STAT_TX_INPROGRESS,
	MRAM_STAT_WREN_CLR,
	MRAM_STAT_ERROR							// TODO: what specific errors are there and what too do now ???? -> reinit SSP ???
} mram_status_t;

// variables
volatile bool 	busyFlag;
mram_status_t 	mramStatus;
uint8_t			*mramData;
uint32_t		mramAdr;
uint32_t		mramLen;
void (*mramCallback)(mram_res_t result, uint32_t adr, uint8_t *data, uint32_t len);


uint8_t tx[4];
uint8_t rx[1];
uint8_t MramReadData[MRAM_MAX_READ_SIZE];
uint8_t MramWriteData[MRAM_MAX_WRITE_SIZE + 4];

// prototypes
void ReadMramCmd(int argc, char *argv[]);
void WriteMramCmd(int argc, char *argv[]);
void ReadMramFinished (mram_res_t result, uint32_t adr, uint8_t *data, uint32_t len);
void WriteMramFinished (mram_res_t result, uint32_t adr, uint8_t *data, uint32_t len);


//
// Be careful here! This Callback is sometimes called from IRQ !!!
// Do not do any complicated logic here!!!
bool MramChipSelect(bool select) {
	Chip_GPIO_SetPinState(LPC_GPIO, MRAM_CS_PORT, MRAM_CS_PIN, !select);
	if (!select) {
		busyFlag = false;
	}
	return true;
}

void MramInit() {
	mramStatus = MRAM_STAT_NOT_INITIALIZED;
	/* --- Chip select IO --- */
	Chip_IOCON_PinMuxSet(LPC_IOCON, MRAM_CS_PORT, MRAM_CS_PIN, IOCON_FUNC0 | IOCON_MODE_INACT);
	Chip_IOCON_DisableOD(LPC_IOCON, MRAM_CS_PORT, MRAM_CS_PIN);
	Chip_GPIO_SetPinDIROutput(LPC_GPIO, MRAM_CS_PORT, MRAM_CS_PIN);
	Chip_GPIO_SetPinState(LPC_GPIO, MRAM_CS_PORT, MRAM_CS_PIN, true);

//	/* Init mram  read Status register
//	 * B7		B6		B5		B4		B3		B2		B1		B0
//	 * SRWD		d.c		d.c		d.c		BP1		BP2		WEL		d.c.
//	 *
//	 * On init all bits should read 0x00.
//	 * The only bit we use here is WEL (Write Enable) and this is reset to 0
//	 * on power up.
//	 * None of the other (protection) bits are used at this moment by this software.
//	 */
	uint8_t *job_status = NULL;
	volatile uint32_t helper;

	/* Read Status register */
	tx[0] = 0x05;
	rx[0] = 0xFF;

	if (ssp_add_job2(SSP_BUS0, tx, 1, rx, 1, &job_status, MramChipSelect))
	{
		/* Error while adding job */
		mramStatus = MRAM_STAT_ERROR;
		return;
	}

	helper = 0;
	while ((*job_status != SSP_JOB_STATE_DONE) && (helper < 1000000)) {
		/* Wait for job to finish */
		helper++;
	}

	if (rx[0] != 0x00) {
		/* Error -  Status Value not as expected */
		mramStatus = MRAM_STAT_ERROR;
		return;
	}
	mramStatus = MRAM_STAT_IDLE;

	RegisterCommand("mRead", ReadMramCmd);
	RegisterCommand("mWrite",WriteMramCmd);
}

void MramMain() {
	if (busyFlag) {
		// TODO: timouts checken
	} else {
		if (mramStatus == MRAM_STAT_RX_INPROGRESS) {
			// Rx job finished.
			mramStatus = MRAM_STAT_IDLE;
			mramCallback(MRAM_RES_SUCCESS, mramAdr, mramData, mramLen);
		} else if (mramStatus == MRAM_STAT_WREN_SET) {
			// Write enable set job finished

			//Initiate write data job
			mramData[0] = 0x02;
			mramData[1] = (mramAdr & 0x00010000) >> 16;
			mramData[2] = (mramAdr & 0x0000ff00) >> 8;
			mramData[3] = (mramAdr & 0x000000ff);

			busyFlag = true;
			if (ssp_add_job2(SSP_BUS0, mramData, mramLen+4, NULL, 0, NULL, MramChipSelect)) {
				/* Error while adding job */
				mramCallback(MRAM_RES_JOB_ADD_ERROR, mramAdr, 0, mramLen);
				return;
			}
			mramStatus = MRAM_STAT_TX_INPROGRESS;

		} else if (mramStatus == MRAM_STAT_TX_INPROGRESS) {
			// Write data job finished
			// Initiate Write Disabled job
			tx[0] = 0x04;

			busyFlag = true;
			if (ssp_add_job2(SSP_BUS0, tx, 1, NULL, 0, NULL, MramChipSelect)) {
				/* Error while adding job */
				mramCallback(MRAM_RES_JOB_ADD_ERROR, mramAdr, 0, mramLen);
				return;
			}
			mramStatus = MRAM_STAT_WREN_CLR;

		} else if (mramStatus == MRAM_STAT_WREN_CLR) {
			// Write disable job finished.
			mramStatus = MRAM_STAT_IDLE;
			mramCallback(MRAM_RES_SUCCESS, mramAdr, mramData, mramLen);
		}
	}
}

void ReadMramCmd(int argc, char *argv[]) {
	if (argc != 2) {
		printf("uasge: cmd <adr> <len> where\n" );
		return;
	}

	// CLI params to binary params
	uint32_t adr = atoi(argv[0]);
	uint32_t len = atoi(argv[1]);
	if (len > MRAM_MAX_READ_SIZE) {
		len = MRAM_MAX_READ_SIZE;
	}

	// Binary Command
	ReadMramAsync(adr, MramReadData, len, ReadMramFinished);
}

void ReadMramFinished (mram_res_t result, uint32_t adr, uint8_t *data, uint32_t len) {
	if (result == MRAM_RES_SUCCESS) {
		printf("MRAM read at %04X:\n", adr);
		for (int i=0; i<len; i++ ) {
			printf("%02X ", ((uint8_t*)data)[i]);
			if ((i+1)%8 == 0) {
				printf("   ");
			}
		}
		printf("\n");
	} else {
		printf("Error reading MRAM: %d\n", result);
	}
}

void WriteMramCmd(int argc, char *argv[]) {
	if (argc != 3) {
		printf("uasge: cmd <adr> <databyte> <len> \n" );
		return;
	}

	// CLI params to binary params
	uint32_t adr = atoi(argv[0]);
	uint8_t byte = atoi(argv[1]);
	uint32_t len = atoi(argv[2]);
	if (len > MRAM_MAX_WRITE_SIZE) {
		len = MRAM_MAX_WRITE_SIZE;
	}

	for (int i=0;i<len;i++){
		MramWriteData[i+4] = byte;		// keep first 4 bytes free for mram Write header.
	}

	// Binary Command
	WriteMramAsync(adr, MramWriteData, len,  WriteMramFinished);
}

void WriteMramFinished (mram_res_t result, uint32_t adr, uint8_t *data, uint32_t len) {
	if (result == MRAM_RES_SUCCESS) {
		printf("%d bytes written to mram at %04X \n", len, adr);
	} else {
		printf("Error reading MRAM: %d\n", result);
	}
}


void ReadMramAsync(uint32_t adr,  uint8_t *rx_data,  uint32_t len, void (*finishedHandler)(mram_res_t result, uint32_t adr, uint8_t *data, uint32_t len)) {
	if (mramStatus != MRAM_STAT_IDLE) {
		finishedHandler(MRAM_RES_BUSY, adr, 0, len);
		return;
	}

	if (rx_data == NULL) {
		finishedHandler(MRAM_RES_DATA_PTR_INVALID, adr, 0, len);
		return;
	}

	if (len > MRAM_MAX_READ_SIZE) {
		finishedHandler(MRAM_RES_RX_LEN_OVERFLOW, adr, 0, len);
		return;
	}

	if (adr + len >  0x020000) {
		finishedHandler(MRAM_RES_INVALID_ADR,  adr, 0, len);
		return;
	}

	/*--- Read Data Bytes Command --- */
	tx[0] = 0x03;
	tx[1] = (adr & 0x00010000) >> 16;
	tx[2] = (adr & 0x0000ff00) >> 8;
	tx[3] = (adr & 0x000000ff);

	busyFlag = true;
	if (ssp_add_job2(SSP_BUS0,  tx, 4, rx_data, len, NULL, MramChipSelect)) {
		/* Error while adding job */
		finishedHandler(MRAM_RES_JOB_ADD_ERROR, adr, 0, len);
		return;
	}

	// next step(s) is/are done in mainloop
	mramStatus = MRAM_STAT_RX_INPROGRESS;
	mramAdr = adr;
	mramLen = len;
	mramData = rx_data;
	mramCallback = 	finishedHandler;
	return;
}

void WriteMramAsync(uint32_t adr, uint8_t *data, uint32_t len,  void (*finishedHandler)(mram_res_t result, uint32_t adr, uint8_t *data, uint32_t len)) {
	if (mramStatus != MRAM_STAT_IDLE) {
		finishedHandler(MRAM_RES_BUSY, adr, 0, len);
		return;
	}

	if (data == NULL) {
		finishedHandler(MRAM_RES_DATA_PTR_INVALID, adr, 0, len);
		return;
	}

	if (len > MRAM_MAX_READ_SIZE) {
		finishedHandler(MRAM_RES_RX_LEN_OVERFLOW, adr, 0, len);
		return;
	}

	if (adr + len >  0x020000) {
		finishedHandler(MRAM_RES_INVALID_ADR,  adr, 0, len);
		return;
	}

	/*--- SetWrtite enable Command --- */

	tx[0] = 0x06;

	busyFlag = true;
	if (ssp_add_job2(SSP_BUS0, tx, 1, NULL, 0, NULL, MramChipSelect)) {
		/* Error while adding job */
		finishedHandler(MRAM_RES_JOB_ADD_ERROR, adr, 0, len);
		return;
	}

	// next step(s) is/are done in mainloop
	mramStatus = MRAM_STAT_WREN_SET;
	mramAdr = adr;
	mramLen = len;
	mramData = data;
	mramCallback = 	finishedHandler;
	return;
}

