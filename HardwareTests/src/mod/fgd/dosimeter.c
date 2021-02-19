/*
 * dosimeter.c
 *
 *  Created on: 11.01.2020
 */

#include <chip.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "dosimeter.h"

#include "../../globals.h"
#include "../../layer1/SPI/spi.h"
#include "../cli/cli.h"
#include "../tim/timer.h"
#include "../fgd/dosimeter.h"


#define FLOGA_VCC_ENABLE_PORT	0
#define FLOGA_VCC_ENABLE_PIN   26

#define FLOGA_FAULT_PORT		1
#define FLOGA_FAULT_PIN		    9

#define FLOGA_IRQ_PORT			1
#define FLOGA_IRQ_PIN		   10

#define FLOGA_CS_PORT			1
#define FLOGA_CS_PIN			4

#define FLOGA_WAIT_INIT_SEC			3		// time in seconds to wait after init (of GPIO pin and CLKOUT for Power Supply)
#define FLOGA_WAIT_MEASURE_SEC		3		// time in seconds between 2 measurements in a 'measure bulk'.


typedef struct flog_registers_s
{
    int8_t   temperature;
    int32_t  sensorValue;
    int32_t  refValue;
    bool     newSensValue;
	bool     newRefValue;
	bool     sensOverflow;
	bool     refOverflow;
	uint8_t  reCharges;
	bool	 reChargeInProgress;
	bool	 reChargeRequested;
}
flog_registers_t;

typedef enum flog_status_e {
	FLOG_STAT_IDLE,

	FLOG_STATE_CHECKINIT,
	FLOG_STAT_READID,
	FLOG_STAT_CFG1,
	FLOG_STAT_CFG2,
	FLOG_STAT_MEASSURE_READDELAY,
	FLOG_STAT_MEASSURE_READ,

	FLOG_STAT_SPIERROR_1,
	FLOG_STAT_SPIERROR_2,
	FLOG_STAT_SPIERROR_3,
	FLOG_STAT_ERROR_CHIPID,

	FLOG_STATE_NOTINITIALIZED,
	FLOG_STAT_ERROR
} flog_status_t;


uint8_t 		RxBuffer[20];
uint8_t 		TxBuffer[20];
bool   			jobFinished;

flog_status_t	status;
uint8_t         valuesToMeassure;
uint8_t			delayTicks;

void ReadAllCmd(int argc, char *argv[]);
void ConfigDeviceCmd(int argc, char *argv[]);
void Switch18VCmd(int argc, char *argv[]);

bool ConfigDevice(bool step1Only);
void LogMeasurementResult();

// This is the callback used by layer1 to activate/deactivate the chip select line for the Floating gate dosimeter.
// Be careful here its a callback from IRQ!!! No long running code pls. ;-)
void ChipSelectFlog(bool select) {
	Chip_GPIO_SetPinState(LPC_GPIO, FLOGA_CS_PORT, FLOGA_CS_PIN, !select);
	if (!select) {
		// Job is finished
		jobFinished = true;
	}
}

void FgdInit() {
	spi_init();

	//	/* --- Chip selects IOs --- */
	Chip_IOCON_PinMuxSet(LPC_IOCON, FLOGA_CS_PORT, FLOGA_CS_PIN, IOCON_FUNC0 | IOCON_MODE_INACT);
	Chip_IOCON_DisableOD(LPC_IOCON, FLOGA_CS_PORT, FLOGA_CS_PIN);
	Chip_GPIO_SetPinDIROutput(LPC_GPIO, FLOGA_CS_PORT, FLOGA_CS_PIN);
	Chip_GPIO_SetPinState(LPC_GPIO, FLOGA_CS_PORT, FLOGA_CS_PIN, true);						// init as 'not selected'

	/* FLOGA_EN is output and switches on the 18V Charge Pump (low active) */
	Chip_IOCON_PinMuxSet(LPC_IOCON, FLOGA_VCC_ENABLE_PORT, FLOGA_VCC_ENABLE_PIN, IOCON_FUNC0 | IOCON_MODE_INACT);
	Chip_IOCON_DisableOD(LPC_IOCON, FLOGA_VCC_ENABLE_PORT, FLOGA_VCC_ENABLE_PIN);
	Chip_GPIO_SetPinDIROutput(LPC_GPIO, FLOGA_VCC_ENABLE_PORT, FLOGA_VCC_ENABLE_PIN);
	Chip_GPIO_SetPinState(LPC_GPIO, FLOGA_VCC_ENABLE_PORT, FLOGA_VCC_ENABLE_PIN, true);		// Init as off

	/* FLOGA_FAULT, FLOGA_IRQ is input */
	Chip_IOCON_PinMuxSet(LPC_IOCON, FLOGA_FAULT_PORT, FLOGA_FAULT_PIN, IOCON_FUNC0 | IOCON_MODE_INACT);
	Chip_IOCON_DisableOD(LPC_IOCON, FLOGA_FAULT_PORT, FLOGA_FAULT_PIN);
	Chip_GPIO_SetPinDIRInput(LPC_GPIO, FLOGA_FAULT_PORT, FLOGA_FAULT_PIN);

	Chip_IOCON_PinMuxSet(LPC_IOCON, FLOGA_IRQ_PORT, FLOGA_IRQ_PIN, IOCON_FUNC0 | IOCON_MODE_INACT);
	Chip_IOCON_DisableOD(LPC_IOCON, FLOGA_IRQ_PORT, FLOGA_IRQ_PIN);
	Chip_GPIO_SetPinDIRInput(LPC_GPIO, FLOGA_IRQ_PORT, FLOGA_IRQ_PIN);

	jobFinished = false;
	RegisterCommand("flogRead", ReadAllCmd);

#if defined RADIATION_TEST
	// Switch on Power generator for 18V supply
	Chip_GPIO_SetPinState(LPC_GPIO, FLOGA_VCC_ENABLE_PORT, FLOGA_VCC_ENABLE_PIN, false);
	// Set Clock out to RTC frequency divided by 1
	Chip_Clock_SetCLKOUTSource(SYSCTL_CLKOUTSRC_RTC, 1);
	Chip_Clock_EnableCLKOUT();

	// Initialize the State mainloop machine to configure the FGD-02F
	status = FLOG_STATE_CHECKINIT;
	delayTicks = FLOGA_WAIT_INIT_SEC * 1000 / TIM_MAIN_TICK_MS;		// give some time for Power supply  to stabilize ....
#else
	// This commands are not allowed during automatic Radiation testing.
	RegisterCommand("flogVcc", Switch18VCmd);
	RegisterCommand("flogConfig", ConfigDeviceCmd);
	status = FLOG_STAT_IDLE;
#endif
}

void ReadAllCmd(int argc, char *argv[]){
	uint8_t probeCount = 1;
	if (argc > 0) {
		probeCount = atoi(argv[0]);
	}
	FgdMakeMeasurement(probeCount);
}

#if ! defined RADIATION_TEST
void ConfigDeviceCmd(int argc, char *argv[]){
//	bool configStep1Only = false;
//	if (argc > 0 && strcmp(argv[0],"FSTO") == 0) {
//		configStep1Only = true;
//	}

	// Trigger the init sequence from here ....
//	if (!ConfigDevice(configStep1Only)) {
//		printf("Error adding SPI Job 1!\n");
//	}
	// Initialize the State mainloop machine to configure the FGD-02F
	// TODO make config step1 only !? ....
	//	status = FLOG_STATE_CHECKINIT;
	//	delayTicks = FLOGA_WAIT_INIT_SEC * 1000 / TIM_MAIN_TICK_MS;		// give some time for Power supply  to stabilize ....

}

void Switch18VCmd(int argc, char *argv[]){
	if (argc == 1) {
		if (strcmp(argv[0],"ON") == 0  || strcmp(argv[0],"on") == 0 ) {
			Chip_GPIO_SetPinState(LPC_GPIO, FLOGA_VCC_ENABLE_PORT, FLOGA_VCC_ENABLE_PIN, false);
			printf("18V switched on!\n");
		} else {
			Chip_GPIO_SetPinState(LPC_GPIO, FLOGA_VCC_ENABLE_PORT, FLOGA_VCC_ENABLE_PIN, true);
			printf("18V switched off!\n");
		}
	}
}
#endif

void FgdMain() {
	// If step needs delay - wait for tick counter to be 0 again.
	if (delayTicks > 0) {
		delayTicks--;
		return;
	}

	// State engine to make config and measurement sequence
	switch (status) {
		// Nothing to do here
		case FLOG_STAT_IDLE:
		case FLOG_STAT_ERROR:
		default:
			break;

		case FLOG_STAT_SPIERROR_1:
		case FLOG_STAT_SPIERROR_2:
		case FLOG_STAT_SPIERROR_3:
			printf("FGD-Spi Error '%d'!\n", status);
			status = FLOG_STAT_ERROR;
			break;

		case FLOG_STAT_ERROR_CHIPID:
			printf("FGD-Not able to read ChipID!\n");
			status = FLOG_STAT_ERROR;
			break;

		case FLOG_STATE_CHECKINIT:
			// We are(were) in first initial wait after reset and power on of the chip.
			// First Thing to do now is to check if the chip is available at all.
			// -> read the CHIPID from register 0x13
			TxBuffer[0] = 0x13 | 0x80 ;  /* Read registers from adr 0x13*/
			if (!spi_add_job(ChipSelectFlog, TxBuffer, 1, RxBuffer, 1)) {
				status = FLOG_STAT_SPIERROR_1;
			} else {
				status = FLOG_STAT_READID;
			}
			break;

		case FLOG_STAT_READID:
			if (!jobFinished) {
				return;				// Wait until job finished
			}
			jobFinished = false;	// Clear the flag.
			if (RxBuffer[0] != 0x01) {
				status = FLOG_STAT_ERROR_CHIPID;
			} else {
				// now we can initialize the FGD Chip.
				TxBuffer[0] = 0x0B | 0x40 ; /* Write registers from adr 0x0B */
				TxBuffer[1] = 0x40;			// REF: 100 .. Referenz freq set to middle value (50kHz?), WINDOW: 11 .. 1 Second measurement window
				TxBuffer[2] = 0x79;			// POWR: 111 .. normal operation, SENS: 001 .. High Sensitivity
				TxBuffer[3] = 0x00;         // ECH: 0 .. recharge not allowed, CHMODE: 00 .. recharging disabled
				TxBuffer[4] = 0x32;         // MNREV: 0 .. no measurement IRQs , NIRQOC: 1 .. open collector, ENGATE: 0 .. window counts clk pulses
				if (!spi_add_job( ChipSelectFlog, TxBuffer, 5, NULL, 0)) {
					status = FLOG_STAT_SPIERROR_2;
				} else {
					status = FLOG_STAT_CFG1;
				}
			}
			break;

		case FLOG_STAT_CFG1:
			if (!jobFinished) {
				return;				// Wait until job finished
			}
			jobFinished = false;	// Clear the flag.

			// next step: write TARGET(7:0)
			TxBuffer[0] = 0x09 | 0x40 ; 	/* Write register  */
			TxBuffer[1] = 0x29;			// This target value was measured at device 'PRIMUS' on 14.1.2020 (ref Values were between 44128 and 44170 (dez) -> bit 17:10 -> 0x2B (44032 ...45055)
			                            // after some playing around the ref value came down to 44000 . -> so I changed the bit 10 to 0 -> 0x2a;
										// second dosimeter on '????' meassured ref values around 40500 on 15.1.2020 -> this would give a TARGET of 0x27.
										// As at this time we can not really assure we know the hw instance, I use the middle value between both here: 0x29 !!!
			if (!spi_add_job( ChipSelectFlog, TxBuffer, 2, NULL, 0)) {
				status = FLOG_STAT_SPIERROR_3;
			} else {
				status = FLOG_STAT_CFG2;
			}
			break;

		case FLOG_STAT_CFG2:
			if (!jobFinished) {
				return;				// Wait until job finished
			}
			jobFinished = false;	// Clear the flag.

			// next step: enable the re-charging system
			TxBuffer[0] = 0x0d | 0x40 ; /* Write register  */
			TxBuffer[1] = 0x41;			// ECH: 1 ... recharge allowed, CHMOD: 01 .. automatic recharging mode
			if (!spi_add_job( ChipSelectFlog, TxBuffer, 2, NULL, 0)) {
				status = FLOG_STAT_SPIERROR_3;
			} else {
				status = FLOG_STAT_IDLE;
				printf("FGD-02F initialized!\n");
			}
			break;

		case FLOG_STAT_MEASSURE_READDELAY:
			/* Delay is over now read all register */
			TxBuffer[0] = 0x00 | 0x80 ; /* Read registers from adr 0x00*/
			if (!spi_add_job( ChipSelectFlog, TxBuffer, 1, RxBuffer, 20)) {
				status = FLOG_STAT_SPIERROR_3;
			} else {
				status = FLOG_STAT_MEASSURE_READ;
			}
			break;

		case FLOG_STAT_MEASSURE_READ:
			if (!jobFinished) {
				return;				// Wait until job finished
			}
			jobFinished = false;	// Clear the flag.

			// Data was read. Make measurement Line
			LogMeasurementResult();

			// TODO: reset recharges register if > 0x14 (was always 0 in tests, prediction how fast/often this will occur: ?????
			valuesToMeassure--;
			if (valuesToMeassure > 0) {
				delayTicks = FLOGA_WAIT_MEASURE_SEC * 1000 / TIM_MAIN_TICK_MS;
				status = FLOG_STAT_MEASSURE_READDELAY;
			} else {
				status = FLOG_STAT_IDLE;
			}
			break;
	}
}

void LogMeasurementResult() {
	// Log the raw data first
	printf("FGD-raw ; ");
	for(int i=0; i<20;i++) {
		printf("%02X ", RxBuffer[i]);
	}
	printf("\n");

	flog_registers_t reg;
	reg.refValue = -1;
	reg.newRefValue = RxBuffer[5] & 0x08;
	reg.refOverflow = RxBuffer[5] & 0x04;
	reg.sensorValue = -1;
	reg.newSensValue =  RxBuffer[8] & 0x08;
	reg.sensOverflow =  RxBuffer[8] & 0x04;
	reg.reCharges = RxBuffer[1] & 0x0F;
	reg.reChargeInProgress = RxBuffer[1] & 0x80;
	reg.reChargeRequested = RxBuffer[1] & 0x60;
	reg.temperature = -40 + RxBuffer[0];		// should be in °C now.!?

	if (!reg.reChargeInProgress) {
		if (reg.newRefValue && !reg.refOverflow) {
			reg.refValue =  (((uint32_t)RxBuffer[5]) & 0x03) << 16 | ((uint32_t)RxBuffer[4])<<8 | ((uint32_t)RxBuffer[3]);
		}

		if (reg.newSensValue && !reg.sensOverflow) {
			reg.sensorValue =  (((uint32_t)RxBuffer[8]) & 0x03) << 16 | ((uint32_t)RxBuffer[7])<<8 | ((uint32_t)RxBuffer[6]);
		}
	}

	printf("FGD-measurement ; ");
	if (reg.reChargeInProgress) {
		printf("no data (recharging)\n");
	} else {
		printf(" %d ; %d ; %ld ; %s ;  %ld ; %s \n",
				reg.temperature,
				reg.reCharges,
				reg.refValue, (reg.refOverflow?"OVL":"."),
				reg.sensorValue, (reg.sensOverflow?"OVL":".")
				);
	}
}

void FgdMakeMeasurement(uint8_t probeCount) {
	if (status == FLOG_STAT_IDLE && probeCount >= 1) {
		valuesToMeassure = probeCount;
		// We start with a 0 second delay
		status = FLOG_STAT_MEASSURE_READDELAY;
		delayTicks = 0;
	}
}
