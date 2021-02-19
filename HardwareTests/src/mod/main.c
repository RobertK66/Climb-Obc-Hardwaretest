/*
 * main.c
 *
 *  Created on: 02.11.2019
 *      Author: Robert
 */
#include <stdio.h>

#include "../globals.h"
#include "main.h"

// include Modules used here
#include "cli/cli.h"
#include "thr/thruster.h"
#include "tim/timer.h"
#include "tim/obc_rtc.h"
#include "mem/eeprom.h"
#include "mem/flash.h"
#include "mem/mram.h"
#include "sen/obc_adc.h"
#include "fgd/dosimeter.h"
#include "sen/temp.h"
#include "com/obc_ttc.h"

#ifdef RADIATION_TEST
#include "rad/radiation_test.h"
#endif
// ddfdd

bool signatureCalcBusy = false;
void(*signatureCalculatedCallback)(FlashSign_t signature) = 0;

#define SIZE32K			0x00007FFF
#define SIZE64K			0x0000FFFF
#define SIZE128K		0x0001FFFF
#define SIZE256K		0x0003FFFF
#define SIZE512K		0x0007FFFF

// prototypes
void WaitForSignatureCalculation();
void CalculateFlashSignatureCmd(int argc, char *argv[]);
void CalculateCmdFinished(FlashSign_t signature);

bool IEC60335_IsEqualSignature(FlashSign_t *sign1, FlashSign_t *sign2)
{
	/* Test the signatures to the expected value */
	if (sign1->word0 != sign2->word0)
		return false;
	if (sign1->word1 != sign2->word1)
		return false;
	if (sign1->word2 != sign2->word2)
		return false;
	if (sign1->word3 != sign2->word3)
		return false;

	/* Return a Flash test Pass */
	return true;
}

// Call all Module Inits
void MainInit() {
	printf("Hello %s HardwareTest with LPCX. Bootmode: %s [%d]\n", BOARD_SHORT, ClimbGetBootmodeStr(), ClimbGetBootmode());
	TimInit();
	RtcInit();
	//AdcInit();
	ThrInit();
	TtcInit();
	//FgdInit();
	//EepromInit();
	//FlashInit();
	//MramInit();
	//TmpInit();
	CliInit();

	LedInit();
#ifdef RADIATION_TEST
	RadTstInit();
#endif

	// Main module own init
	RegisterCommand("crcFlash", CalculateFlashSignatureCmd);
}

// Poll all Modules from Main loop
void MainMain() {
	// Call module mains with 'fast - requirement'
	CliMain();
	ThrMain();
	TtcMain();

//	FlashMain();
//	MramMain();
//	TmpMain();
//	EepromMain();
	bool tick = TimMain();
	if (tick) {
		//ClimbLedToggle(0);
		// Call module mains with 'tick - requirement'
		//EepromMain();
		RtcMain();			// At this moment we only track day changes here so Tick time is enough.
		LedMain();
#ifdef RADIATION_TEST
		RadTstMain();
#endif
	//	FgdMain();
	}

	// Main modules own functions
	WaitForSignatureCalculation();

//  Test timer delay function....
//	static uint32_t counter = 0;
//	if ((counter++ % 100000) == 0) {
//		ClimbLedToggle(0);
//		TimBlockMs(10);
//		ClimbLedToggle(0);
//	}

}

void WaitForSignatureCalculation() {
	if (signatureCalcBusy) {
		if (!Chip_FMC_IsSignatureBusy()) {
			/* signature was generated */
			signatureCalcBusy = false;
			Chip_FMC_ClearSignatureBusy();

			// Callback gets struct on stack
			FlashSign_t signature;
			signature.word0 = LPC_FMC->FMSW[0];
			signature.word1 = LPC_FMC->FMSW[1];
			signature.word2 = LPC_FMC->FMSW[2];
			signature.word3 = LPC_FMC->FMSW[3];
			signatureCalculatedCallback(signature);
		}
	}
}

void CalculateFlashSignatureAsync(uint32_t startAddr, uint32_t length, void(*SignatureCalculated)(FlashSign_t signature)) {
	if (!signatureCalcBusy) {
		signatureCalcBusy = true;
		Chip_FMC_ComputeSignature(startAddr, startAddr + length);
		signatureCalculatedCallback = SignatureCalculated;
	}
}

void CalculateFlashSignatureCmd(int argc, char *argv[]) {
	long temp;
	uint32_t start = 0x00000000;
	uint32_t len   = SIZE256K;
	if (argc > 0) {
		temp = strtol(argv[0], NULL, 0);			// This allows also to enter '0x...' values.
		if ((temp > 0) && (temp < SIZE512K)) {
			start = temp;
		}
	}
	if (argc > 1) {
		temp = strtol(argv[1], NULL, 0);
		if ((temp > 0) && (start + temp <= SIZE512K)) {
			len = temp;
		}
	}
	if (start + len > SIZE512K) {
		len = SIZE512K - start;
	}

	printf("Calculating Flash Signature 0x%06X - 0x%06X (len: 0x%06X)\n", start, start + len, len);
	CalculateFlashSignatureAsync(start, len, CalculateCmdFinished);
}

void CalculateCmdFinished(FlashSign_t signature) {
	printf("Signature calculated: %08X / %08X / %08X / %08X\n", signature.word0,signature.word1,signature.word2,signature.word3 );
}
