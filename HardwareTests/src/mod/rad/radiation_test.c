/*
 * radiation_test.c
 *
 *  Created on: 03.01.2020
 */

#include <stdio.h>
#include <chip.h>

#include <cr_section_macros.h>

#include "radiation_test.h"
#include "radtst_memory.h"
#include "radtst_extmemory.h"


#include "../tim/timer.h"
#include "../tim/obc_rtc.h"
#include "../cli/cli.h"
#include "../main.h"		// for Flash signature

#include "../../hw_obc/obc_board.h" 		// For watchdog feed pin
#include "../sen/obc_adc.h"
#include "../fgd/dosimeter.h"
#include "../sen/temp.h"
#include "../mem/mram.h"


#define RADTST_SEQ_SENSOR_REPORT_SECONDS				4			// Send all sensor values every 5 seconds

#define RADTST_SEQ_LOGBERRY_WATCHDOG_SECONDS			60			// Send Watchdog message every 60 seconds
#define RADTST_SEQ_REPORTLINE_SECONDS				   300			// print out a report line with all check and error counters.

#define RADTST_SEQ_DOSIMETER_REPORT_SECONDS				10

#define RADTST_SEQ_READCHECKS_SECONDS					15			// Initiate all read checks every n seconds
#define RADTST_SEQ_WRITECHECKS_SECONDS				  2400			// Initiate all write checks every n seconds

#define RADTST_SEQ_RESET_READ_EXPECTATIONS_SECONDS	   600			// Every 10 minutes we make a new baseline for the expected read values

#define RADTST_FLASHSIG_PARTS	4										// Program Flash Check is divided in 4 Sections ..
#define RADTST_PART_FLASHSIZE	(0x0007FFFF / RADTST_FLASHSIG_PARTS)	// .. each having this size.

typedef struct radtst_workload_s {
	// Read processes running (async to mainloop call)
	unsigned int radtest_caclulate_flashsig 	:1;
	unsigned int  	:1;
	unsigned int  	:1;
	unsigned int  	:1;
	unsigned int  	:1;
	unsigned int  	:1;
	unsigned int  	:1;
	unsigned int  	:1;

	// Write processes running (async to mainloop call)
	unsigned int  	:1;
	unsigned int  	:1;
	unsigned int  	:1;
	unsigned int  	:1;
	unsigned int  	:1;
	unsigned int  	:1;
	unsigned int  	:1;
	unsigned int  	:1;

	// Rebase processes running (async to mainloop call)
	unsigned int  radtest_rebase_flashsig		:1;
	unsigned int  	:1;
	unsigned int  	:1;
	unsigned int  	:1;
	unsigned int  	:1;
	unsigned int  	:1;
	unsigned int  	:1;
	unsigned int  	:1;
} radtest_radtst_workload_t;



// MOdule variables
uint32_t 			radtstTicks = 0;
radtst_counter_t	radtstCounter;
bool 				verboseExpectations;
bool 				reportLineWithHeader;
radtest_radtst_workload_t	runningBits;
radtst_readcheckenabled_t	readEnabled;

// Compare data for different memory types
static uint8_t rtRtcGprExpectedData[20];


//static bool 		expSigReadActive = false;
uint8_t		flashPartIdx = 0;
FlashSign_t 	expectedFlashSig[RADTST_FLASHSIG_PARTS];
uint8_t 		*expectedPagePatternsPtr; // points fillpattern bytes[0..3]
uint16_t		initFirstMainloopTicks;

#define RADTST_MRAM_TARGET_PAGESIZE		MRAM_MAX_WRITE_SIZE						// 1k pages ->
//#define RADTST_MRAM_TARGET_PAGES		(128 * 1024) / MRAM_MAX_WRITE_SIZE		// 128k available



// ***************** Special Compiler Linker Constructs to get following code/variables in other sections of the LPC
// Special Structures for different memory sections and test patterns
#define RADTST_RAM2_TARGET_PAGESIZE		1024	// 1k pages
#define RADTST_RAM2_TARGET_PAGES		32		// gives 32 pages to fill up RAM2 memory block

__DATA(RAM2) uint8_t ram2Target[RADTST_RAM2_TARGET_PAGES][RADTST_RAM2_TARGET_PAGESIZE] ; 		// create an initialised 1k buffer in RAM2 (0x2007C000 - 0x2007C7FF)



// prototypes
void RadTstLogberryWatchdog();
void RadTstResetReadExpectations();
void RadTstExpectedSigCalculated(FlashSign_t signature);
void RadTstLogReadError(radtst_sources_t source, uint8_t *expPtr, uint8_t *actPtr, uint16_t len);
//void RadTstLogReadError2(radtst_sources_t source, uint8_t expByte, uint8_t *actPtr, uint16_t len);
void RadTstCheckRtcGpr();
void RadTstCheckRam2();
void RadTstWriteRam2();
void RadTstCheckPrgFlash();
void RadTstSigCalculated(FlashSign_t signature);
void RadTstPrintReportLine();
void RadTstInitRam2Content();



void RadTstProvokeErrorCmd(int argc, char *argv[]);

void RadTstInit(void) {
	verboseExpectations = true;				// After reset the calculated expected values can write to output once.
	expectedPagePatternsPtr = &expectedPagePatternsSeq[0];	// Reset the expected Pattern pointer to start value (possible: 0 ...3)

	RadTstInitRam2Content();
	RadTstResetReadExpectations();			// initialize all read expectations

	readEnabled.rtcGpr   = true;			// this checks can be started immediate
	readEnabled.ram2	 = true;			// 				"
	readEnabled.prgFlash = false;			// this has to wait until the initial Checksum Expectation is finalized.
	readEnabled.mram	 = false;			//       "                the firstwrite pattern was finalized.

	RegisterCommand("simErr", RadTstProvokeErrorCmd);
	initFirstMainloopTicks = 25;				// this triggers the write memory after the first x mailoop ticks only once
}

void RadTstInitRam2Content() {
	for (int page = 0; page< RADTST_RAM2_TARGET_PAGES; page++) {
		for (int x =0; x < RADTST_RAM2_TARGET_PAGESIZE; x++) {
			uint8_t expByte = expectedPagePatternsPtr[page % RADTST_EXPECTED_PATTERN_CNT];
			ram2Target[page][x] = expByte;
		}
	}
}

void RadTstResetReadExpectations() {
	printf("Radtest reset all read expectations\n");
	// Rtc GPRs
	// --------
	if (!RtcIsGprChecksumOk()) {
		// lets write (an unused) byte to get the Checksum corrected.
		RtcWriteGpr(12, 55);
	}
	// read all 20 bytes to compare when Checksum lost again.
	RtcReadAllGprs(rtRtcGprExpectedData);
	if (verboseExpectations) {
		printf("Current GPR Content: ");
		for (int i=0;i<20;i++) {
			printf("%02X ", rtRtcGprExpectedData[i]);
		}
		printf("\n");
	}

	// Flash Checksum
	// ______________
	if (!runningBits.radtest_caclulate_flashsig) {
		flashPartIdx = 0;
		readEnabled.prgFlash = false;		// Stop the checking routine until Checksum is recalculated.
		CalculateFlashSignatureAsync(0x000000, RADTST_PART_FLASHSIZE, RadTstExpectedSigCalculated);
	} else {
		radtstCounter.signatureRebaseBlocked++;
	}

	// RAM Content
	// -----------
	for (int page = 0; page< RADTST_RAM2_TARGET_PAGES; page++) {
		for (int x =0; x < RADTST_RAM2_TARGET_PAGESIZE; x++) {
			uint8_t expByte = expectedPagePatternsPtr[page%RADTST_EXPECTED_PATTERN_CNT];
			if (ram2Target[page][x] != expByte) {
				ram2Target[page][x] = expByte;
				// TODO: Log this error!?
				radtstCounter.expRam2BytesChanged++;	// TODO: auf bit counter umstellen
			}
		}
	}
}

void RadTstMain(void) {
	radtstTicks++;
	if ((radtstTicks % (RADTST_SEQ_LOGBERRY_WATCHDOG_SECONDS * 1000 / TIM_MAIN_TICK_MS))  == 0) {
		ObcLedStacieAIo(1,true);
		RadTstLogberryWatchdog();
		ObcLedStacieAIo(1,false);
	}
	if ((radtstTicks % (RADTST_SEQ_SENSOR_REPORT_SECONDS * 1000 / TIM_MAIN_TICK_MS))  == 0) {
		// Transmit sensor values and reset onboard watchdog every 4 seconds
		ObcWdtFeedSet(true);
		read_transmit_sensors();
		tmp_read_temperature();
		ObcWdtFeedSet(false);
	}
	if ((radtstTicks % (RADTST_SEQ_DOSIMETER_REPORT_SECONDS * 1000 / TIM_MAIN_TICK_MS))  == 0) {
		// Read all registers every RADTST_SEQ_DOSIMETER_REPORT_SECONDS
		FgdMakeMeasurement(1);
	}

	if ((radtstTicks % (RADTST_SEQ_REPORTLINE_SECONDS * 1000 / TIM_MAIN_TICK_MS))  == 0) {
		RadTstPrintReportLine();
	}
	if ((radtstTicks % (RADTST_SEQ_RESET_READ_EXPECTATIONS_SECONDS * 1000 / TIM_MAIN_TICK_MS))  == 0) {
		verboseExpectations = false;
		RadTstResetReadExpectations();
	}


	if (initFirstMainloopTicks > 1) {
		readEnabled.ram2	= false;		// Disable the read check until write is done
		readEnabled.mram 	= false;
		readEnabled.i2cmem 	= false;
		//readEnabled.flash12	= false;
		initFirstMainloopTicks--;
	}

	if (((radtstTicks % (RADTST_SEQ_WRITECHECKS_SECONDS * 1000 / TIM_MAIN_TICK_MS))  == 0)
	    || (initFirstMainloopTicks == 1)) {
		// We start first mainloop only once with immediately  writing to all memory target areas.
		initFirstMainloopTicks = 0;
		// increment to next page pattern base pointer
		expectedPagePatternsPtr++;
		if (expectedPagePatternsPtr > &expectedPagePatternsSeq[3]) {
			expectedPagePatternsPtr = &expectedPagePatternsSeq[0];
		}

		printf("New Write Page Patterns[0..3]: %02X %02X %02X %02X\n", expectedPagePatternsPtr[0], expectedPagePatternsPtr[1], expectedPagePatternsPtr[2], expectedPagePatternsPtr[3]);

		readEnabled.ram2	 = false;		// Disable the read check until write is done
		RadTstWriteRam2();

		readEnabled.mram 	= false;
		RadTstWriteMram();

		readEnabled.i2cmem 	= false;
		RadTstWriteFram();

//		readEnabled.flash12	= false;
//		RadTstWriteFlash();
	}

	// read checks have lower prio if requested in same mainloop tick. Disabled by above write tests inits...
	if ((radtstTicks % (RADTST_SEQ_READCHECKS_SECONDS * 1000 / TIM_MAIN_TICK_MS))  == 0) {
			if (readEnabled.rtcGpr) {
				RadTstCheckRtcGpr();
			}
			if (readEnabled.ram2) {
				RadTstCheckRam2();
			}
			if (readEnabled.prgFlash) {
				RadTstCheckPrgFlash();
			}
			if (readEnabled.mram) {
				RadTstCheckMram();
			}
			if (readEnabled.i2cmem) {
				RadTstCheckFram();
			}

//			if (readEnabled.flash12) {
//				RadTstCheckFlash();
//			}
		}
}

void RadTstLogberryWatchdog() {
//	printf("RunningBits: %d%d%d%d%d%d%d%d", 0,0,0,0,0,0,0,runningBits.radtest_rebase_flashsig);
//	printf(" %d%d%d%d%d%d%d%d", 0,0,0,0,0,0,0,0);
//	printf(" %d%d%d%d%d%d%d%d\n", 0,0,0,0,0,0,0,runningBits.radtest_caclulate_flashsig);
	printf("Supervision watchdog feed\n");
}

void RadTstCheckRtcGpr() {
	uint8_t actualData[20];
	radtstCounter.rtcgprTestCnt++;

	if (!RtcIsGprChecksumOk()) {
		// Checksum error. Lets count individual bit errors.
		radtstCounter.rtcgprTestErrors++;
		RtcReadAllGprs(actualData);
		RadTstLogReadError(RADTST_SRC_RTCGPR, rtRtcGprExpectedData, actualData, 20);
	}
}

void RadTstCheckPrgFlash() {
	// We only start the check, if no rebase of expected value is ongoing.
	if (!runningBits.radtest_rebase_flashsig) {
		runningBits.radtest_caclulate_flashsig = TRUE;
		flashPartIdx = 0;
		CalculateFlashSignatureAsync(0x000000, RADTST_PART_FLASHSIZE, RadTstSigCalculated);
	} else {
		radtstCounter.signatureCheckBlocked++;
	}
}

void RadTstSigCalculated(FlashSign_t signature) {
	radtstCounter.signatureCheckCnt++;
	if (! IEC60335_IsEqualSignature(&expectedFlashSig[flashPartIdx], &signature)) {
		radtstCounter.signatureErrorCnt++;
		// TODO: if upper part of flash we can scan for differences from expected to actual
		//       to find out how many bits where flipped ....
	}
	flashPartIdx++;
	if (flashPartIdx < RADTST_FLASHSIG_PARTS) {
		CalculateFlashSignatureAsync(RADTST_PART_FLASHSIZE*flashPartIdx, RADTST_PART_FLASHSIZE, RadTstSigCalculated);
	} else {
		runningBits.radtest_caclulate_flashsig = FALSE;
	}
}



void RadTstLogWriteError2(radtst_sources_t source, uint8_t expByte, uint8_t *actPtr, uint16_t len) {
	uint16_t diffCntBits  = 0;
	uint16_t diffCntBytes = 0;

	for (uint16_t idx = 0; idx < len; idx++ ) {
		if (expByte != actPtr[idx]) {
			diffCntBytes++;
			uint8_t bitErrors = expByte ^ actPtr[idx];
			while (bitErrors) {
				diffCntBits += bitErrors & 1;
		        bitErrors >>= 1;
		    }
		}
	}

	if (diffCntBytes > 0) {
		printf("WriteError from %d: %d bits in %d/%d bytes\n", source, diffCntBits, diffCntBytes, len);
	}

}


void RadTstLogReadError2(radtst_sources_t source, uint8_t pageNr, uint8_t expByte, uint8_t *actPtr, uint16_t len) {
	uint16_t diffCntBits  = 0;
	uint16_t diffCntBytes = 0;

	for (uint16_t idx = 0; idx < len; idx++ ) {
		if (expByte != actPtr[idx]) {
			diffCntBytes++;
			uint8_t bitErrors = expByte ^ actPtr[idx];
			while (bitErrors) {
				diffCntBits += bitErrors & 1;
		        bitErrors >>= 1;
		    }
		}
	}

	if (diffCntBytes > 0) {
		printf("*** ReadError from %d [%d]: %d bits in %d/%d bytes\n", source, pageNr,  diffCntBits, diffCntBytes, len);
	}

}


void RadTstLogReadError(radtst_sources_t source, uint8_t *expPtr, uint8_t *actPtr, uint16_t len) {
	uint16_t diffCntBits  = 0;
	uint16_t diffCntBytes = 0;

	for (uint16_t idx = 0; idx < len; idx++ ) {
		if (expPtr[idx] != actPtr[idx]) {
			diffCntBytes++;
			uint8_t bitErrors = expPtr[idx] ^ actPtr[idx];
			while (bitErrors) {
				diffCntBits += bitErrors & 1;
		        bitErrors >>= 1;
		    }
		}
	}

	if (diffCntBytes > 0) {
		printf("ReadError from %d: %d bits in %d/%d bytes\n", source, diffCntBits, diffCntBytes, len);
	}
}


void RadTstCheckRam2() {
	for (int page = 0; page< RADTST_RAM2_TARGET_PAGES; page++) {
		radtstCounter.ram2PageReadCnt++;
		for (int x =0; x < RADTST_RAM2_TARGET_PAGESIZE; x++) {
			uint8_t expByte = expectedPagePatternsPtr[page%RADTST_EXPECTED_PATTERN_CNT];
			bool pageError = false;
			if (ram2Target[page][x] != expByte) {
				pageError = true;
			}
			if (pageError) {
				radtstCounter.ram2PageReadError++;
				RadTstLogReadError2(RADTST_SRC_RAM2, page, expByte, &(ram2Target[page][0]), RADTST_RAM2_TARGET_PAGESIZE);
			}
		}
	}
}


void RadTstWriteRam2() {
	// Write the new Patterns
	for (int page = 0; page< RADTST_RAM2_TARGET_PAGES; page++) {
		radtstCounter.ram2PageWriteCnt++;
		for (int x =0; x < RADTST_RAM2_TARGET_PAGESIZE; x++) {
			uint8_t expByte = expectedPagePatternsPtr[page% RADTST_EXPECTED_PATTERN_CNT];
			ram2Target[page][x] = expByte;
		}
	}
	// And Check if ok
	//bool error = false;
	for (int page = 0; page< RADTST_RAM2_TARGET_PAGES; page++) {
		for (int x =0; x < RADTST_RAM2_TARGET_PAGESIZE; x++) {
			uint8_t expByte = expectedPagePatternsPtr[page%RADTST_EXPECTED_PATTERN_CNT];
			bool pageError = false;
			if (ram2Target[page][x] != expByte) {
				pageError = true;
			}
			if (pageError) {
		//		error = true;
				radtstCounter.ram2PageWriteError++;
				RadTstLogWriteError2(RADTST_SRC_RAM2, expByte, &(ram2Target[page][0]), RADTST_RAM2_TARGET_PAGESIZE);
			}
		}
	}
	// TODO: only enable if write was possible without erors !???
	//if (!error) {
	readEnabled.ram2	 = true;
	//}
}


//uint8_t pageBuffer[RADTST_MRAM_TARGET_PAGESIZE + 4];
//uint8_t curPage;
//void RadWriteMramFinished(mram_res_t result, uint32_t adr, uint8_t *data, uint32_t len);

//void RadTstWriteMram() {
//	// Write the new Patterns
//	curPage = 0;
//	radtstCounter.mramPageWriteCnt++;
//	//printf("MRAM Write all pages started\n");
//
//	uint8_t expByte = expectedPagePatternsPtr[curPage % RADTST_EXPECTED_PATTERN_CNT];
//	for (int x =0; x < RADTST_MRAM_TARGET_PAGESIZE; x++) {
//		pageBuffer[4 + x] = expByte;
//	}
//	WriteMramAsync(curPage * RADTST_MRAM_TARGET_PAGESIZE, pageBuffer, RADTST_MRAM_TARGET_PAGESIZE, RadWriteMramFinished );
//}
//
//void RadWriteMramFinished(mram_res_t result, uint32_t adr, uint8_t *data, uint32_t len) {
//	if (result != MRAM_RES_SUCCESS) {
//		radtstCounter.mramPageWriteError++;
//		printf("MRAM write bus error %d.\n", result);
////      TODO: keine Ahnung, was hier besser/wahrscheinlich besser ist/wäre :-((((
////		readEnabled.mram = true;		// Maybe this will work later ...
////		return;							// but to continue with another page write makes no sense !?
//	}
//	curPage++;
//	if (curPage < RADTST_MRAM_TARGET_PAGES) {
//		uint8_t expByte = expectedPagePatternsPtr[curPage % RADTST_EXPECTED_PATTERN_CNT];
//		for (int x =0; x < RADTST_MRAM_TARGET_PAGESIZE; x++) {
//			pageBuffer[4 + x] = expByte;
//		}
//		WriteMramAsync(curPage * RADTST_MRAM_TARGET_PAGESIZE, pageBuffer, RADTST_MRAM_TARGET_PAGESIZE, RadWriteMramFinished );
//	} else {
//		// All pages written restart read tests
//		readEnabled.mram = true;
//		printf("MRAM Write all pages ended.\n");
//	}
//}


//uint8_t curReadPage;
//void RadReadMramFinished(mram_res_t result, uint32_t adr, uint8_t *data, uint32_t len);

//void RadTstCheckMram() {
//	curReadPage = 0;
//	//printf("MRAM read test started\n");
//	runningBits.radtest_mramread_running = true;
//	radtstCounter.mramPageReadCnt++;
//
//	ReadMramAsync(curReadPage * RADTST_MRAM_TARGET_PAGESIZE, pageBuffer, RADTST_MRAM_TARGET_PAGESIZE, RadReadMramFinished );
//}
//
//void RadReadMramFinished(mram_res_t result, uint32_t adr, uint8_t *data, uint32_t len) {
//	if (result != MRAM_RES_SUCCESS) {
//		printf("MRAM read bus Error %d\n", result);
//		radtstCounter.mramPageReadError++;
//	}
//	uint8_t expByte = expectedPagePatternsPtr[curReadPage % RADTST_EXPECTED_PATTERN_CNT];
//	bool error = false;
//	for (int x = 0; x < RADTST_MRAM_TARGET_PAGESIZE; x++) {
//		if (data[x] != expByte) {
//			error = true;
//		}
//	}
//	if (error) {
//		radtstCounter.mramPageReadError++;
//		RadTstLogReadError2(RADTST_SRC_MRAM, expByte, &data[0], RADTST_MRAM_TARGET_PAGESIZE );
//	}
//
//	curReadPage++;
//	if (curReadPage < RADTST_MRAM_TARGET_PAGES) {
//		ReadMramAsync(curReadPage * RADTST_MRAM_TARGET_PAGESIZE, pageBuffer, RADTST_MRAM_TARGET_PAGESIZE, RadReadMramFinished );
//	} else {
//		//printf("MRAM read test stopped\n");
//		runningBits.radtest_mramread_running = false;
//	}
//}
//




void RadTstExpectedSigCalculated(FlashSign_t signature) {
	if (!IEC60335_IsEqualSignature(&expectedFlashSig[flashPartIdx], &signature)) {
		// TODO: Log this error!?
		radtstCounter.expSignatureChanged++;
		expectedFlashSig[flashPartIdx] = signature;
	}
	flashPartIdx++;
	if (flashPartIdx < RADTST_FLASHSIG_PARTS) {
		CalculateFlashSignatureAsync(RADTST_PART_FLASHSIZE*flashPartIdx, RADTST_PART_FLASHSIZE, RadTstExpectedSigCalculated);
	} else {
		runningBits.radtest_rebase_flashsig = false;
		readEnabled.prgFlash = true;							// Checks can be started now.
		if (verboseExpectations) {
			printf("Expected FlashSignatures:\n");
			for (int i=0; i<RADTST_FLASHSIG_PARTS; i++) {
				printf("Part[%d]: %08X / %08X / %08X / %08X\n", i,
						expectedFlashSig[i].word0,
						expectedFlashSig[i].word1,
						expectedFlashSig[i].word2,
						expectedFlashSig[i].word3 );
			}
		}
	}
}

void RadTstPrintReportLine() {
	uint32_t *ptr = (uint32_t *)&radtstCounter;
	uint8_t	 cnt = sizeof(radtstCounter) / 4;

	printf("radtstCounter ; %ld ; %ld ; %ld ;",rtc_get_date(), rtc_get_time(), secondsAfterReset);
	for (int i=0;i<cnt;i++) {
		printf(" %ld ;", ptr[i]);
	}
	printf("\n");
}


void DummyCallback(mram_res_t result, uint32_t adr, uint8_t *data, uint32_t len){};

void RadTstProvokeErrorCmd(int argc, char *argv[]) {
	int arg0 = 0;
	if (argc > 0) {
		arg0 = atoi(argv[0]);
	}
	uint32_t *gprbase = (uint32_t *)(&(LPC_RTC->GPREG));
	uint32_t gprx;
	if (arg0 == 3) {
		// provoke an mram error
		uint8_t bytes[5];
		bytes[4] = 0x12;	// This is always wrong -> some bit errors depending of current expected pattern
		WriteMramAsync(0x010A00, bytes, 1, DummyCallback);
	} else if (arg0 == 2) {
		// RAM2 error (2bits)
		ram2Target[3][12] = ram2Target[3][12] ^ 0x41;
	}
	else if (arg0 == 1) {
		// RTC GPR RAM Flip 3 bit in 2 words
		gprx = *(gprbase + 1);
		gprx ^= (1 << 7);
		gprx ^= (1 << 13);
		*(gprbase + 1) = gprx;
		gprx = *(gprbase + 3);
		gprx ^= (1 << 22);
		*(gprbase + 3) = gprx;
	} else {
		// RTC GPR RAM Flip a single bit
		gprx = *(gprbase + 1);
		gprx ^= (1 << 7);
		*(gprbase + 1) = gprx;
	}
}





