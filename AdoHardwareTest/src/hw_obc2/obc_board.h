/*
 * obc_board.h
 *
 *  Created on: 02.11.2019
 *      Author: Robert
 */

#ifndef HW_OBC_OBC_BOARD_H_
#define HW_OBC_OBC_BOARD_H_

#include "chip.h"

/* Onboard I2C bus and conected adresses */
#define ONBOARD_I2C				LPC_I2C1
#define I2C_ADR_EEPROM1				0x57
#define I2C_ADR_EEPROM2				0x53
#define I2C_ADR_EEPROM3				0x51
#define I2C_ADR_TEMP				0x48		// !!! its not correct on schematics !!!
#define I2C_ADR_FRAM				0x50


// memory device ID
typedef enum {memEeprom1, memEeprom2, memEeprom3, memFram } memdev_t;
// memory device Names
#define C_MEM_EEPROM1_NAME 		"EE1"
#define C_MEM_EEPROM2_NAME 		"EE2"
#define C_MEM_EEPROM3_NAME 		"EE3"
#define C_MEM_FRAM_NAME 		"FRA"



// Module API (all as Aliases pointing to implementation)
#define ClimbBoardInit 			ObcClimbBoardInit
#define ClimbBoardSystemInit 	ObcClimbBoardSystemInit
#define ClimbGetBootmode()		ObcGetBootmode()
#define ClimbGetBootmodeStr()	ObcGetBootmodeStr()

#define ClimbLedToggle(x)		ObcLedToggle(x)
#define ClimbLedSet(x,y)		ObcLedSet(x,y)
#define ClimbLedTest(x)			ObcLedTest(x)

#define GetI2CAddrForMemoryDeviceName(x) 	ObcGetI2CAddrForMemoryDeviceName(x);

// definitions only available in OBC version. For other boards Alias for ObcGetBootmode() gives a pure number (int).
typedef enum {DebugEven, DebugOdd, Even, Odd} bootmode_t;

// Module Implementation Prototypes
void ObcClimbBoardInit();
void ObcClimbBoardSystemInit();
bootmode_t ObcGetBootmode();
char* ObcGetBootmodeStr();

void ObcLedToggle(uint8_t ledNr);
void ObcLedSet(uint8_t ledNr,  bool On);
bool ObcLedTest(uint8_t ledNr);
void ObcSpSupplySet(uint8_t sp, bool On);
void ObcWdtFeedSet(bool On);
char ObcGetSupplyRail();
bool ObcGetRbfIsInserted();
void ObcLedStacieAIo(uint8_t io, bool On);

uint8_t ObcGetI2CAddrForMemoryDeviceName(char* name);

#endif /* HW_OBC_OBC_BOARD_H_ */
