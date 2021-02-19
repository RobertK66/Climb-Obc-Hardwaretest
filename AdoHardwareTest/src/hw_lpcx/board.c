/*
 * board.c
 *
 *  This is the Board Abstraction Layer for the LPCXpresso 1769 Developer board.
 *
 *  Created on: 02.11.2019
 *      Author: Robert
 */

#include "lpcx_board.h"
#include <mod/cli.h>
//#include "../layer1/I2C/obc_i2c.h"
//#include <ado_uart.h>

#define LED0_GPIO_PORT_NUM	0
#define LED0_GPIO_BIT_NUM   22

STATIC const PINMUX_GRP_T pinmuxing[] = {
	{0,  2,   IOCON_MODE_INACT | IOCON_FUNC1},	/* TXD0 "Uart D" used (on OBC) for Thruster RS485 here normal UART without enable pin */
	{0,  3,   IOCON_MODE_INACT | IOCON_FUNC1},	/* RXD0 "Uart D" used (on OBC) for Thruster RS485 here normal UART without enable pin */
	{2,  0,   IOCON_MODE_INACT | IOCON_FUNC2},	/* TXD1 "Uart C" same pins as OBC*/
	{2,  1,   IOCON_MODE_INACT | IOCON_FUNC2},	/* RXD1 "Uart C" same pins as OBC*/
	{0,  10,  IOCON_MODE_INACT | IOCON_FUNC1},	/* TXD2 "Uart B" ! on OBC this pins are used as SPI ! here we use them as CLI Uart*/
	{0,  11,  IOCON_MODE_INACT | IOCON_FUNC1},	/* RXD2 "Uart B" ! on OBC this pins are used as SPI ! here we use them as CLI Uart*/
	{0,  0,   IOCON_MODE_INACT | IOCON_FUNC2},	/* TXD3 "UART A" same pins as OBC*/
	{0,  1,   IOCON_MODE_INACT | IOCON_FUNC2},	/* RXD3 "UART A" same pins as OBC*/

	{0,  4,   IOCON_MODE_INACT | IOCON_FUNC2},	/* CAN-RD2 */
	{0,  5,   IOCON_MODE_INACT | IOCON_FUNC2},	/* CAN-TD2 */
	{LED0_GPIO_PORT_NUM, LED0_GPIO_BIT_NUM ,  IOCON_MODE_INACT | IOCON_FUNC0},	/* Led red */
	{0,  23,  IOCON_MODE_INACT | IOCON_FUNC1},	/* ADC 0 */
	{0,  26,  IOCON_MODE_INACT | IOCON_FUNC2},	/* DAC */

	/* ENET */
	{0x1, 0,  IOCON_MODE_INACT | IOCON_FUNC1},	/* ENET_TXD0 */
	{0x1, 1,  IOCON_MODE_INACT | IOCON_FUNC1},	/* ENET_TXD1 */
	{0x1, 4,  IOCON_MODE_INACT | IOCON_FUNC1},	/* ENET_TX_EN */
	{0x1, 8,  IOCON_MODE_INACT | IOCON_FUNC1},	/* ENET_CRS */
	{0x1, 9,  IOCON_MODE_INACT | IOCON_FUNC1},	/* ENET_RXD0 */
	{0x1, 10, IOCON_MODE_INACT | IOCON_FUNC1},	/* ENET_RXD1 */
	{0x1, 14, IOCON_MODE_INACT | IOCON_FUNC1},	/* ENET_RX_ER */
	{0x1, 15, IOCON_MODE_INACT | IOCON_FUNC1},	/* ENET_REF_CLK */
	{0x1, 16, IOCON_MODE_INACT | IOCON_FUNC1},	/* ENET_MDC */
	{0x1, 17, IOCON_MODE_INACT | IOCON_FUNC1},	/* ENET_MDIO */
	{0x1, 27, IOCON_MODE_INACT | IOCON_FUNC1},	/* CLKOUT */

	/* Joystick buttons. */
	{2, 3,  IOCON_MODE_INACT | IOCON_FUNC0},	/* JOYSTICK_UP */
	{0, 15, IOCON_MODE_INACT | IOCON_FUNC0},	/* JOYSTICK_DOWN */
	{2, 4,  IOCON_MODE_INACT | IOCON_FUNC0},	/* JOYSTICK_LEFT */
	{0, 16, IOCON_MODE_INACT | IOCON_FUNC0},	/* JOYSTICK_RIGHT */
	{0, 17, IOCON_MODE_INACT | IOCON_FUNC0},	/* JOYSTICK_PRESS */

	{0, 27, IOCON_MODE_INACT | IOCON_FUNC1},	/* I2C0 SDA */		// this has nothing connected (only pull ups) and is
	{0, 28, IOCON_MODE_INACT | IOCON_FUNC1},	/* I2C0 SCL */      // available on PAD10/PAD16
	{0, 19, IOCON_MODE_INACT | IOCON_FUNC3},	/* I2C1 SDA */		// this connects to eeprom with adr 0x50
	{0, 20, IOCON_MODE_INACT | IOCON_FUNC3},	/* I2C1 SCL */      // and is also on PAD2/PAD8


	{3,  25,  IOCON_MODE_INACT | IOCON_FUNC0},	/* Led green */
	{3,  26,  IOCON_MODE_INACT | IOCON_FUNC0},	/* Led blue */

};

//
// Public API Implementation
//

// This routine is called prior to main(). We setup all pin Functions and Clock settings here.
void LpcxClimbBoardSystemInit() {

	Chip_IOCON_SetPinMuxing(LPC_IOCON, pinmuxing, sizeof(pinmuxing) / sizeof(PINMUX_GRP_T));
	Chip_SetupXtalClocking();		// Asumes 12Mhz Quarz -> PLL0 frq=384Mhz -> CPU frq=96MHz

	/* Setup FLASH access to 4 clocks (100MHz clock) */
	Chip_SYSCTL_SetFLASHAccess(FLASHTIM_100MHZ_CPU);
}


// This routine is called from main() at startup (prior entering main loop).
void LpcxClimbBoardInit() {
	/* Initializes GPIO */
	Chip_GPIO_Init(LPC_GPIO);

	/* Initialize IO Dirs */
	// TODO: make a nice loop here (like/or combine as the function selects on all IOS...)
	/* Pin PIO0_22 is configured as GPIO pin during SystemInit */
	/* Set the PIO_22 as output */
	Chip_GPIO_WriteDirBit(LPC_GPIO, LED0_GPIO_PORT_NUM, LED0_GPIO_BIT_NUM, true);
	Chip_GPIO_WriteDirBit(LPC_GPIO, 3, 25, true);
	Chip_GPIO_WriteDirBit(LPC_GPIO, 3, 26, true);

	// Decide the UART to use for command line interface.
	CliInit1(LPC_UART2);		// UART2 - J2 Pin 40/41 on LPCXpresso 1769 Developer board.


	// Init I2c bus for Onboard device(s) (1xEEProm)
	//InitOnboardI2C(ONBOARD_I2C);

}

void LpcxLedToggle(uint8_t ledNr) {
	if (ledNr == 0) {
		LpcxLedSet(ledNr, !LpcxLedTest(ledNr));
	}
}

/* Sets the state of a board LED to on or off */
void LpcxLedSet(uint8_t ledNr, bool On)
{
	/* There is only one LED */
	if (ledNr == 0) {
		Chip_GPIO_WritePortBit(LPC_GPIO, LED0_GPIO_PORT_NUM, LED0_GPIO_BIT_NUM, On);
	}
}

/* Returns the current state of a board LED */
bool LpcxLedTest(uint8_t ledNr)
{
	bool state = false;
	if (ledNr == 0) {
		state = Chip_GPIO_ReadPortBit(LPC_GPIO, LED0_GPIO_PORT_NUM, LED0_GPIO_BIT_NUM);
	}
	return state;
}
