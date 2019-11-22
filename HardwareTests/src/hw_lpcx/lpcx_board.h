/*
 * lpcx_board.h
 *
 *  Created on: 02.11.2019
 *      Author: Robert
 */

#ifndef HW_LPCX_LPCX_BOARD_H_
#define HW_LPCX_LPCX_BOARD_H_

#include "chip.h"

// Module API (all as Aliases pointing to own implementation)
#define ClimbBoardInit 			LpcxClimbBoardInit
#define ClimbBoardSystemInit	LpcxClimbBoardSystemInit
#define ClimbGetBootmode()		0
#define ClimbGetBootmodeStr()	"not available"

#define ClimbLedToggle(x)		LpcxLedToggle(x)
#define ClimbLedSet(x,y)		LpcxLedSet(x,y)
#define ClimbLedTest(x)			LpcxLedTest(x)

// Module Implementation Prototypes
void LpcxClimbBoardSystemInit();
void LpcxClimbBoardInit();
int LpcxGetBootMode();

void LpcxLedToggle(uint8_t ledNr);
void LpcxLedSet(uint8_t ledNr,  bool On);
bool LpcxLedTest(uint8_t ledNr);

#endif /* HW_LPCX_LPCX_BOARD_H_ */
