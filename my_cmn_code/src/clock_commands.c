/*
 * clock_commands.c
 *
 *  Created on: 23.01.2019
 *      Author: Robert
 */

#include <stdlib.h>

#include "chip.h"

#include "my_board_api.h"
#include "clock_commands.h"

void Print(char* str, uint32_t val) {
	char vstring [33];
	itoa (val,vstring,10);
	Board_UARTPutSTR(str);
	Board_UARTPutSTR(vstring);
}

void Println(char* str, uint32_t val) {
	Print(str,val);
	Board_UARTPutChar('\n');
}


void ClkInfo(void ) {
	uint32_t val;

	CHIP_SYSCTL_PLLCLKSRC_T mpl = Chip_Clock_GetMainPLLSource();
	if (mpl == SYSCTL_PLLCLKSRC_IRC) {
		Board_UARTPutSTR("IRC -> SysClk\n");

		val = Chip_Clock_GetIntOscRate();
		Print("OscRate: ", val/1000000);
		Board_UARTPutSTR(" MHz\n");

	} else if (mpl == SYSCTL_PLLCLKSRC_MAINOSC) {
		Board_UARTPutSTR("XTAL -> SysClk\n");
	} else if (mpl == SYSCTL_PLLCLKSRC_RTC) {
		Board_UARTPutSTR("RTC -> SysClk\n");
	} else {
		Board_UARTPutSTR("??? -> SysClk\n");
	}

	val = Chip_Clock_GetMainPLLInClockRate();
	Print("PLLInRate: ", val/1000000);
	Board_UARTPutSTR(" MHz\n");

	CHIP_SYSCTL_CCLKSRC_T ct = Chip_Clock_GetCPUClockSource();
	if (ct == SYSCTL_CCLKSRC_MAINPLL) {
		Board_UARTPutSTR("PLL used\n");
		val = Chip_Clock_GetMainPLLOutClockRate();
		Print("PLLOutRate: ", val/1000000);
		Board_UARTPutSTR(" MHz\n");
	} else {
		Board_UARTPutSTR("PLL not used ");
	}

	val = Chip_Clock_GetMainClockRate();
	Print("MainClockRate: ", val/1000000);
	Board_UARTPutSTR(" MHz\n");

	val = Chip_Clock_GetCPUClockDiv();
	Println("Divider: ", val);

	val = Chip_Clock_GetSystemClockRate();
	Print("SysClockrate: ",val/1000000);
	Board_UARTPutSTR(" MHz\n");

	val = Chip_Clock_GetRTCClockRate();
	Println("RtcClockrate: ",val);

	val = Chip_Clock_GetRTCOscRate();
	Println("RtcOsckrate: ",val);
}


cmdresult_t ClockCmd(int argc, char** argv) {
	ClkInfo();
//	if (argc == 1 && (strcmp(argv[0], "info") == 0)) {
//		ClkInfo();
//	} else {
//		Board_UARTPutSTR("possible commands are: info\n");
//
//
//	}
	return cmdOk;
}

