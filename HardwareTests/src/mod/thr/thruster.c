/*
 * thruster.c
 *
 *  Created on: 11.11.2019
 */

// ... include C std dependencies if needed ....
#include <stdio.h>			// use if needed (e.g. for printf, ...)
#include <string.h>			// use if needed (e.g. for strcpy, str... )
#include <ring_buffer.h>	// TODO: replace with own implementation (with random size ant not 2^x as this one!)

// ... include dependencies on other modules if needed ....
// #include "..\..\globals.h"   // use if needed
#include "../../layer1/UART/uart.h"
#include "../cli/cli.h"
#include "thruster.h"

// module defines
#define THR_HELLO_STR	"Hi there.\n"
#define THRUSTER_UART	LPC_UART0					// This is the same in OBC and LPCX board
#define THR_TXBUFFER_SIZE 64						// the biggest command should be good here

// module prototypes (local routines not in public API)
void ThrUartIrq(LPC_USART_T *pUart);
void ThrusterSend(char *str);
void ThrusterSendCmd(int argc, char *argv[]);
void ThrusterPrintStatusCmd(int argc, char *argv[]);

// module variables
int myStateExample;
int thrTxError;

bool thrtxInProgress = false;
char prtThrTxBuffer[THR_TXBUFFER_SIZE];
RINGBUFF_T thrTxRingbuffer;

// module function implementations
// -------------------------------
// Module Init - here goes all module init code to be done before mainloop starts.
void ThrInit() {
	RingBuffer_Init(&thrTxRingbuffer,(void *)prtThrTxBuffer, 1, THR_TXBUFFER_SIZE);

	// initialize the thruster UART ....
	InitUart(THRUSTER_UART, 9600, ThrUartIrq);

	// register module commands with cli
	RegisterCommand("trSend", ThrusterSendCmd);
	RegisterCommand("trStat", ThrusterPrintStatusCmd);

	myStateExample = 0;
	thrTxError = 0;

	// Init communications to the thruster if needed ....
	ThrusterSend(THR_HELLO_STR);
}

// thats the 'mainloop' call of the module
void ThrMain() {
	// do your stuff here. But remember not to make 'wait' loops or other time consuming operations!!!
	// Its called 'Cooperative multitasking' so be kind to your sibling-modules and return as fast as possible!
	if (myStateExample++ % 800000 == 0) {
		// Note printf() does not take too much time here, but keep the texts small and do not float the CLI (its slow)!
		//printf("Hello this goes to CLI UART!\n");
	}

}

// An example CLI Command to trigger thruster communication
void ThrusterSendCmd(int argc, char *argv[]){
	char *tx = "ABCabc";
	if (argc>0) {
		tx = argv[0];
	}
	ThrusterSend(tx);
}

void ThrusterPrintStatusCmd(int argc, char *argv[]) {
	printf("Thruster TxErrors: %d\n", thrTxError);
}


// Send content of string to Thruster UART
void ThrusterSend(char *str) {
	//Chip_GPIO_SetPinOutLow(LPC_GPIO, 3, 25);
	int len = strlen(str);
	if (RingBuffer_InsertMult(&thrTxRingbuffer, (void*)str, len) != len) {
		// Tx Buffer is to small to hold all bytes
		thrTxError++;
	}
	if (!thrtxInProgress) {
		// Trigger to send the first byte and enable the TxEmptyIRQ
		char c;
		thrtxInProgress = true;
		RingBuffer_Pop(&thrTxRingbuffer, &c);
		Chip_UART_SendByte(THRUSTER_UART, c);
		Chip_UART_IntEnable(THRUSTER_UART, UART_IER_THREINT);
	}
	//Chip_GPIO_SetPinOutHigh(LPC_GPIO, 3, 25);
}

void ThrUartIrq(LPC_USART_T *pUart){
	Chip_GPIO_SetPinOutLow(LPC_GPIO, 3, 26);
	uint32_t irqid = pUart->IIR;
	if (( irqid & UART_IIR_INTSTAT_PEND ) == 0) {
		// There is an Irq pending
		if (( irqid & (UART_IIR_INTID_RLS) ) == 0 ) {
			// This was a line status-error IRQ
//			uint32_t ls = pUart->LSR;		// clear this pending IRQ

		} else if ((irqid & (UART_IIR_INTID_RDA || UART_IIR_INTID_CTI )) != 0) {
			// This was a "Rx Fifo treshhold reached" or a "char timeout" IRQ -> Bytes are available in RX FIFO to be processsed
//			uint8_t rbr = pUart->RBR;

			// not used (yet?) in thruster module
		} else if ((irqid & (UART_IIR_INTID_THRE)) != 0) {
			// The Tx FIFO is empty (now). Lets fill it up. It can hold up to 16 (UART_TX_FIFO_SIZE) bytes.
			char c;
			int  i = 0;
			while( i++ < UART_TX_FIFO_SIZE) {
				if (RingBuffer_Pop(&thrTxRingbuffer, &c) == 1) {
					Chip_UART_SendByte(THRUSTER_UART, c);
				} else {
					// We have to stop because our tx ringbuffer is empty now.
					thrtxInProgress = false;
					Chip_UART_IntDisable(THRUSTER_UART, UART_IER_THREINT);
					break;
				}
			}
		}
	}
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 3, 26);
}

