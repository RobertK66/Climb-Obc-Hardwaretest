/*
 * board_impl.c
 *
 * This is the Board Abstraction Layer for the OBC EM1 Hardware.
 *
 *  Created on: 02.11.2019
 *      Author: Robert
 */

#include <string.h>

#include "obc_board.h"
#include <mod/cli.h>



#define LED_GREEN_WD_GPIO_PORT_NUM               1
#define LED_GREEN_WD_GPIO_BIT_NUM               18
#define LED_BLUE_RGB_GPIO_PORT_NUM               2
#define LED_BLUE_RGB_GPIO_BIT_NUM                6

#define BOOT_SELECT_GPIO_PORT_NUM                0
#define BOOT_SELECT_GPIO_BIT_NUM                29

#define DEBUG_SELECT_GPIO_PORT_NUM               0
#define DEBUG_SELECT_GPIO_BIT_NUM                5

#define SP1_VCC_EN_PIN							28
#define SP1_VCC_EN_PORT							1
#define SP2_VCC_EN_PIN							7
#define SP2_VCC_EN_PORT							2
#define SP3_VCC_EN_PIN							15
#define SP3_VCC_EN_PORT							1
#define SP4_VCC_EN_PIN							22
#define SP4_VCC_EN_PORT							1

#define SUPPLY_RAIL_PIN 						25
#define SUPPLY_RAIL_PORT 						3

#define RBF_PIN									21
#define RBF_PORT								0

#define STACIE_A_IO1_PIN						21
#define STACIE_A_IO1_PORT						1


void SwitchVccFfgDCmd(int argc, char *argv[]);
/* Pin muxing configuration */
STATIC const PINMUX_GRP_T pinmuxing[] = {
	// UARTS
	{0,  2,   IOCON_MODE_INACT | IOCON_FUNC1},	/* TXD0 - UART SP-D	Y+ */
	{0,  3,   IOCON_MODE_INACT | IOCON_FUNC1},	/* RXD0 - UART SP-D Y+ */
	{2,  0,   IOCON_MODE_INACT | IOCON_FUNC2},	/* TXD1 - UART SP-C X- */
	{2,  1,   IOCON_MODE_INACT | IOCON_FUNC2},	/* RXD1 - UART SP-C X- */
	{2,  8,   IOCON_MODE_INACT | IOCON_FUNC2},	/* TXD2 - UART SP-B Y- */
	{2,  9,   IOCON_MODE_INACT | IOCON_FUNC2},	/* RXD2 - UART SP-B Y- */
	{0,  0,   IOCON_MODE_INACT | IOCON_FUNC2},	/* TXD3 - UART SP-A X+ */
	{0,  1,   IOCON_MODE_INACT | IOCON_FUNC2},	/* RXD3 - UART SP-A X+ */

	// LEDS
	{2,  6,  IOCON_MODE_INACT | IOCON_FUNC0},	/* Led 4 Blue - also RGB LED Pin */
	{1, 18,  IOCON_MODE_INACT | IOCON_FUNC0},	/* Led 1 green - Watchdog Feed   */

	// GPIOs
	{0,  29, IOCON_MODE_INACT | IOCON_FUNC0},	/* BL_SEL1    */
	{0,  30, IOCON_MODE_INACT | IOCON_FUNC0},	/* ...    */

	{0,   5, IOCON_MODE_INACT | IOCON_FUNC0},	/* Debug_SEL2 */

	{1, 27, IOCON_MODE_INACT | IOCON_FUNC1},	/* CLOCKOUT */

	{0, 27, IOCON_MODE_INACT | IOCON_FUNC1},	/* I2C0 SDA */
	{0, 28, IOCON_MODE_INACT | IOCON_FUNC1},	/* I2C0 SCL */

	{0, 19, IOCON_MODE_INACT | IOCON_FUNC3},	/* I2C1 SDA */
	{0, 20, IOCON_MODE_INACT | IOCON_FUNC3},	/* I2C1 SCL */

	{0, 10, IOCON_MODE_INACT | IOCON_FUNC2},	/* I2C2 SDA */
	{0, 11, IOCON_MODE_INACT | IOCON_FUNC2},	/* I2C2 SCL */

	//{0,  26,  IOCON_MODE_INACT | IOCON_FUNC0},	/* FLOGA_EN  */

};


//
// Public API Implementation
//

// This routine is called prior to main(). We setup all pin Functions and Clock settings here.
void Obc1ClimbBoardSystemInit() {
	Chip_IOCON_SetPinMuxing(LPC_IOCON, pinmuxing, sizeof(pinmuxing) / sizeof(PINMUX_GRP_T));
	Chip_SetupXtalClocking();		// Asumes 12Mhz Quarz -> PLL0 frq=384Mhz -> CPU frq=96MHz

	/* Setup FLASH access to 4 clocks (100MHz clock) */
	Chip_SYSCTL_SetFLASHAccess(FLASHTIM_100MHZ_CPU);
}

// This routine is called from main() at startup (prior entering main loop).
void Obc1ClimbBoardInit() {
	/* Initializes GPIO */
	Chip_GPIO_Init(LPC_GPIO);

	/* Initialize IO Dirs */
	// TODO: make a nice loop here (like/or combine as the function selects on all IOS...)
	/* The LED Pins are configured as GPIO pin (FUNC0) during SystemInit */
	// Here we define them as OUTPUT
	Chip_GPIO_WriteDirBit(LPC_GPIO, LED_GREEN_WD_GPIO_PORT_NUM, LED_GREEN_WD_GPIO_BIT_NUM, true);
	Chip_GPIO_WriteDirBit(LPC_GPIO, LED_BLUE_RGB_GPIO_PORT_NUM, LED_BLUE_RGB_GPIO_BIT_NUM, true);
	//Chip_GPIO_WriteDirBit(LPC_GPIO, 0, 26, true);


	// Die Boot bits sind inputs
	Chip_GPIO_WriteDirBit(LPC_GPIO, DEBUG_SELECT_GPIO_PORT_NUM, DEBUG_SELECT_GPIO_BIT_NUM, false);
	Chip_GPIO_WriteDirBit(LPC_GPIO, BOOT_SELECT_GPIO_PORT_NUM, BOOT_SELECT_GPIO_BIT_NUM, false);
	Chip_GPIO_WriteDirBit(LPC_GPIO, 0, 30, false);

	// Supply rail status input
	Chip_GPIO_WriteDirBit(LPC_GPIO, SUPPLY_RAIL_PORT, SUPPLY_RAIL_PIN, false);

	// UART for comand line interface init
	CliInit1(LPC_UART2);		// We use SP - B (same side as JTAG connector) as Debug UART.);

	// Init I2c bus for Onboard devices (3xEEProm, 1xTemp, 1x FRAM)
	//InitOnboardI2C(ONBOARD_I2C);

	// Sidepanel supply swiches (outputs)
	Chip_GPIO_WriteDirBit(LPC_GPIO, SP1_VCC_EN_PORT, SP1_VCC_EN_PIN, true);
	Chip_GPIO_WriteDirBit(LPC_GPIO, SP2_VCC_EN_PORT, SP2_VCC_EN_PIN, true);
	Chip_GPIO_WriteDirBit(LPC_GPIO, SP3_VCC_EN_PORT, SP3_VCC_EN_PIN, true);
	Chip_GPIO_WriteDirBit(LPC_GPIO, SP4_VCC_EN_PORT, SP4_VCC_EN_PIN, true);

	// Enable supply to all sidepanels
	Obc1SpSupplySet(1, true);
	Obc1SpSupplySet(2, true);
	Obc1SpSupplySet(3, true);
	Obc1SpSupplySet(4, true);

	// Stacie A GPIO -> set as output for rad-tests
	Chip_GPIO_WriteDirBit(LPC_GPIO, STACIE_A_IO1_PORT, STACIE_A_IO1_PIN, true);


	// Feed watchdog
	Obc1WdtFeedSet(true);
	Obc1WdtFeedSet(false);


	//Chip_GPIO_SetPinState(LPC_GPIO, 0, 26, false);
}


void Obc1LedToggle(uint8_t ledNr) {
	if (ledNr == 0) {
		Obc1LedSet(ledNr, !Obc1LedTest(ledNr));
	}
}


/* Sets the state of a board LED to on or off */
void Obc1LedSet(uint8_t ledNr, bool On)
{
	/* There is only one LED */
	if (ledNr == 0) {
		Chip_GPIO_WritePortBit(LPC_GPIO, LED_BLUE_RGB_GPIO_PORT_NUM, LED_BLUE_RGB_GPIO_BIT_NUM, On);
	}
}


/* Returns the current state of a board LED */
bool Obc1LedTest(uint8_t ledNr)
{
	bool state = false;
	if (ledNr == 0) {
		state = Chip_GPIO_ReadPortBit(LPC_GPIO, LED_BLUE_RGB_GPIO_PORT_NUM, LED_BLUE_RGB_GPIO_BIT_NUM);
	}
	return state;
}


bootmode_t Obc1GetBootmode(){
	bool boot = Chip_GPIO_ReadPortBit(LPC_GPIO, BOOT_SELECT_GPIO_PORT_NUM, BOOT_SELECT_GPIO_BIT_NUM);
	if (Chip_GPIO_ReadPortBit(LPC_GPIO, DEBUG_SELECT_GPIO_PORT_NUM, DEBUG_SELECT_GPIO_BIT_NUM)) {
		 if (boot) {
			return DebugEven;
		 } else {
			 return DebugOdd;
		 }
	} else {
		if (boot) {
			return Even;
		 } else {
			 return Odd;
		 }
	}
}

char* Obc1GetBootmodeStr() {
	switch (Obc1GetBootmode()) {
		case Odd:
			return "Odd";
		case Even:
			return "Even";
		case DebugOdd:
			return "DebugOdd";
		case DebugEven:
			return "DebugEven";
	}
	return "Unknown";
}

uint8_t Obc1GetI2CAddrForMemoryDeviceName(char* name) {
    if (strcmp(name,C_MEM_EEPROM1_NAME) == 0) {
    	return I2C_ADR_EEPROM1;
    } else if (strcmp(name,C_MEM_EEPROM2_NAME) == 0) {
    	return I2C_ADR_EEPROM2;
    } else if (strcmp(name,C_MEM_EEPROM3_NAME) == 0) {
    	return I2C_ADR_EEPROM3;
    } else if (strcmp(name,C_MEM_FRAM_NAME) == 0) {
    	return I2C_ADR_FRAM;
    }

    return 0;
}

/* Sets the state of the specified supply panel switch to on or off (logic low on pin turns on output) */
void Obc1SpSupplySet(uint8_t sp, bool On)
{
	/* There is only one LED */
	if (sp == 1) {
		Chip_GPIO_WritePortBit(LPC_GPIO, SP1_VCC_EN_PORT, SP1_VCC_EN_PIN, !On);
	}
	else if (sp == 2) {
		Chip_GPIO_WritePortBit(LPC_GPIO, SP2_VCC_EN_PORT, SP2_VCC_EN_PIN, !On);
	}
	else if (sp == 3) {
		Chip_GPIO_WritePortBit(LPC_GPIO, SP3_VCC_EN_PORT, SP3_VCC_EN_PIN, !On);
	}
	else if (sp == 4) {
		Chip_GPIO_WritePortBit(LPC_GPIO, SP4_VCC_EN_PORT, SP4_VCC_EN_PIN, !On);
	}
}

/* Sets the state of the watchdog feed pin to on/off */
void Obc1WdtFeedSet(bool On)
{
	Chip_GPIO_WritePortBit(LPC_GPIO, LED_GREEN_WD_GPIO_PORT_NUM, LED_GREEN_WD_GPIO_BIT_NUM, On);
}

/* Reads the status of the active supply rail */
char Obc1GetSupplyRail(){

	if (Chip_GPIO_ReadPortBit(LPC_GPIO, SUPPLY_RAIL_PORT, SUPPLY_RAIL_PIN))
	{
		return 'A';
	}
	else
	{
		return 'C';
	}

}
/* Returns true if the RBF is inserted */
bool Obc1GetRbfIsInserted(){

	if (Chip_GPIO_ReadPortBit(LPC_GPIO, RBF_PORT, RBF_PIN))
	{
		return true;
	}
	else
	{
		return false;
	}
}

/* Sets the state of the STACIE A IO-X PIN */
void Obc1LedStacieAIo(uint8_t io, bool On)
{
	if (io == 1) {
		Chip_GPIO_WritePortBit(LPC_GPIO, STACIE_A_IO1_PORT, STACIE_A_IO1_PIN, On);
	}
}



