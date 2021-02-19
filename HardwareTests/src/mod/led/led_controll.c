/*
 * led_controll.c
 *
 *  Created on: Nov 5, 2020
 *      Author: jevgeni
 */
#include "led_controll.h"

#include <chip.h>
#include <stdio.h>

#include "../../mod/cli/cli.h"


void MyOwnFunction(int argc, char *argv[]){
	printf("Hiiiii my new command");
	//Chip_GPIO_SetPinOutLow(LPC_GPIO, 0, 22); //turn blue on
	Chip_GPIO_SetPinToggle(LPC_GPIO, 0, 22);
}

void LedInit(){

	//configure IO bins to output
	Chip_GPIO_WriteDirBit(LPC_GPIO, 0, 22, true);
	Chip_GPIO_WriteDirBit(LPC_GPIO, 3, 25, true);
	Chip_GPIO_WriteDirBit(LPC_GPIO, 3, 26, true);

	// set all leds to off
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 3, 26);
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 0, 22);
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 3, 25);

	RegisterCommand("testCMD", MyOwnFunction); //you register commands here

}

static int ledCounter = 0;

void LedMain(){
	ledCounter++;
	if (ledCounter % 1 == 0){
		Chip_GPIO_SetPinToggle(LPC_GPIO, 3, 25);
	}
}
