/*
===============================================================================
 Name        : AdoHardwareTest.c
 Author      : $(author)
 Version     :
 Copyright   : $(copyright)
 Description : main definition
===============================================================================
*/

#if defined (__USE_LPCOPEN)
#if defined(NO_BOARD_LIB)
#include "chip.h"
#else
#include "board.h"
#endif
#endif

#include "globals.h"
#include <cr_section_macros.h>

#include <stdio.h>
#include <mod/cli.h>

// TODO: insert other definitions and declarations here

int main(void) {

#if defined (HW_USED)
	// Our own board abstraction.
	ClimbBoardInit();
#endif

    // Read clock settings and update SystemCoreClock variable
    SystemCoreClockUpdate();
    // TODO: insert code here

    printf("Hello ADO World");
    // Enter an infinite loop, just incrementing a counter
    while(1) {
    	CliMain();
    }
    return 0 ;
}
