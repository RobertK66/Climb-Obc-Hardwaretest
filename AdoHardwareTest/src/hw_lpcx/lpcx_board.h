/*
 * lpcx_board.h
 *
 *  Created on: 02.11.2019
 *      Author: Robert
 */

#ifndef HW_LPCX_LPCX_BOARD_H_
#define HW_LPCX_LPCX_BOARD_H_

#include "chip.h"

/* Onboard I2C adresses */
#define ONBOARD_I2C				LPC_I2C1			// ist doich die selbe wie am obc
#define I2C_ADR_EEPROM1				0x50			// lpcx hat nur ein eeprom

//typedef enum {memEeprom1 } memdev_t;
//#define C_MEM_EEPROM1_NAME 		"EE1"

// Module API (all as Aliases pointing to own implementation)
#define ClimbBoardInit 			LpcxClimbBoardInit
#define ClimbBoardSystemInit	LpcxClimbBoardSystemInit
#define ClimbGetBootmode()		0
#define ClimbGetBootmodeStr()	"not available"

#define ClimbLedToggle(x)		LpcxLedToggle(x)
#define ClimbLedSet(x,y)		LpcxLedSet(x,y)
#define ClimbLedTest(x)			LpcxLedTest(x)

#define GetI2CAddrForMemoryDeviceName(x) I2C_ADR_EEPROM1		// no need to implement code here its obvious, we only have one memory chip.

// Module Implementation Prototypes
void LpcxClimbBoardSystemInit();
void LpcxClimbBoardInit();
int LpcxGetBootMode();

void LpcxLedToggle(uint8_t ledNr);
void LpcxLedSet(uint8_t ledNr,  bool On);
bool LpcxLedTest(uint8_t ledNr);

#endif /* HW_LPCX_LPCX_BOARD_H_ */
