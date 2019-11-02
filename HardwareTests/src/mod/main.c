/*
 * main.c
 *
 *  Created on: 02.11.2019
 *      Author: Robert
 */

#include "main.h"

// include Modules used here
#include "..\globals.h"
#include "cli\cli.h"


static int i = 0;


// Call all Module Inits
void MainInit() {
	printf("Hello Climb HardwareTest.");
	CliInit();
}

// Poll all Modules from Main loop
void MainMain() {

	CliMain();
	i++ ;
	if (i % 100000 == 0) {
		ClimbLedToggle(0);
		//printf(".");
	}
}

