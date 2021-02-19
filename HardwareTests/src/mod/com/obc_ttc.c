/*
 * obc_ttc.c
 *
 *  Created on: 02.03.2020
 */

#include <string.h>
#include <stdio.h>

#include "../../layer1/UART/uart.h"
//#include "/home/jevgeni/climb_base/Climb-Obc-Hardwaretest/HardwareTests/src/layer1/UART/uart.h"
#include "../cli/cli.h"
#include "../crc/obc_checksums.h"

#include "obc_ttc.h"

#define TTC_TXBUFFER_SIZE	64			// This must always be power of 2 !!!!!
#define TTC_RXBUFFER_SIZE   50
#define TTC_A_UART			LPC_UART3	//
#define TTC_C_UART			LPC_UART1	//

// Pegasus Protocol defines
#define TTC_OP_TRANSMIT     0x07
#define TTC_OP_RECEIVE      0x1E
#define TTC_OP_OPMODE       0x2A
#define TTC_OP_GETTELEMETRY 0x2D

#define TTC_ACTION_ACK   0x00
#define TTC_ACTION_EXEC  0x4B
#define TTC_ACTION_NACK  0x7F

#define TTC_PAYLOAD_FULL_SIZE 46 		// 46 bytes of payload including pid, source and destination excluding function and action code byte and data crc byte.

#define TTC_TELEMETRY_TRX1_TEMP	         1
#define TTC_TELEMETRY_TRX2_TEMP			 2
//#define TTC_TELEMETRY_RST_COUNT          3
#define TTC_TELEMETRY_MODE              4
#define TTC_TELEMETRY_VERSION           5
#define TTC_TELEMETRY_RSSI              6



// 'new defined' structures and constants

typedef enum ttc_protid_e
{
	TTC_PRID_TRANSMIT_ACK,
	TTC_PRID_TRANSMIT_NAK,
	TTC_PRID_OPMODE_ACK,
	TTC_PRID_OPMODE_NAK,
	TTC_PRID_OPMODE,
	TTC_PRID_GSRECEIVED,
	TTC_PRID_TELEMETRY_ACK
} ttc_protid_t;


typedef struct ttc_prot_s
{
	ttc_protid_t	ProtId;
	uint8_t			DataLen;
	uint8_t			*pData;
} ttc_prot_t;

typedef enum ttc_rxstatus_e
{
	TTC_RX_IDLE,
	TTC_RX_TRANSMIT_C,
	TTC_RX_OPMODE_C,
	TTC_RX_OPMODE_DATA,
	TTC_RX_RECEIVE_C,
	TTC_RX_RECEIVE_DATA,
	TTC_RX_TELEMETRY_C,
	TTC_RX_TELEMETRY_ID,
	TTC_RX_TELEMETRY_DATA
} ttc_rxstatus_t;

typedef struct TTC_S
{
	LPC_USART_T *pUart;
	volatile bool TxInProgress;
	RINGBUFF_T TxRingbuffer;
	char TxBuffer[TTC_TXBUFFER_SIZE];
	ttc_rxstatus_t RxState;
	uint8_t RxDataBuffer[TTC_RXBUFFER_SIZE];
	int8_t	RxExpectedDataBytes;
	uint8_t	RxIdx;
	uint8_t	RxChecksum;
	volatile bool PackageReceived;
	ttc_prot_t RxPackage;
} TTC_T;


TTC_T	ttca;
TTC_T	ttcc;

int error = 0;

void TtcUartIrqA(LPC_USART_T *pUart);
void TtcUartIrqC(LPC_USART_T *pUart);
void TtcUartIrq(TTC_T *pTTC);


void TtcSendBytesA(uint8_t *pBytes, int len);
void TtcSendBytesC(uint8_t *pBytes, int len);
void TtcSendBytes(TTC_T *pTtc, uint8_t *pBytes, int len);

void TtcSendCmd(int argc, char *argv[]);
void TtcStatusCmd(int argc, char *argv[]);


void TtcInit() {
	ttca.pUart = TTC_A_UART;
	ttca.TxInProgress = false;
	ttca.PackageReceived = false;
	ttca.RxState = TTC_RX_IDLE;

	ttcc.pUart = TTC_C_UART;
	ttcc.TxInProgress = false;
	ttcc.PackageReceived = false;
	ttcc.RxState = TTC_RX_IDLE;

	RingBuffer_Init(&ttca.TxRingbuffer,(void *)ttca.TxBuffer, 1, TTC_TXBUFFER_SIZE);
	RingBuffer_Init(&ttcc.TxRingbuffer,(void *)ttcc.TxBuffer, 1, TTC_TXBUFFER_SIZE);

	InitUart(TTC_A_UART, 9600, TtcUartIrqA);
	InitUart(TTC_C_UART, 9600, TtcUartIrqC);

	Chip_UART_IntEnable(TTC_A_UART, UART_IER_RLSINT | UART_IER_RBRINT);
	Chip_UART_IntEnable(TTC_C_UART, UART_IER_RLSINT | UART_IER_RBRINT);

	RegisterCommand("ttcSend", TtcSendCmd);
	RegisterCommand("ttcStatus", TtcStatusCmd);
}

void TtcMain() {
	if (ttca.PackageReceived) {
		uint8_t protid = ttca.RxPackage.ProtId;
		ttca.PackageReceived = false;
		printf("TTC A: received %d\n",protid);
	}
	if (ttcc.PackageReceived) {
		uint8_t protid = ttcc.RxPackage.ProtId;
		ttcc.PackageReceived = false;
		printf("TTC C: received %d\n",protid);
	}
}

void TtcSendCmd(int argc, char *argv[]){
	bool sendToC = false;
	char *sendString = "ABCDEFG";
	if (argc > 1) {
		if (strcmp(argv[0],"C") == 0) {
			sendToC = true;
		}
		sendString = argv[1];
	} else if (argc == 1) {
		sendString = argv[0];
	}

	if (sendToC) {
		TtcSendBytesC((uint8_t *)sendString, strlen(sendString));
	} else {
		TtcSendBytesA((uint8_t *)sendString, strlen(sendString));
	}
}

void TtcStatusCmd(int argc, char *argv[]){
	printf("TTC Errors: %d\n", error);
}

static inline void TtcPackageReceivedIRQ(TTC_T *pTtc, ttc_protid_t id, uint8_t *data, uint8_t dataLen) {
	if (!pTtc->PackageReceived) {
		pTtc->PackageReceived = true;
		pTtc->RxPackage.ProtId = id;
		pTtc->RxPackage.DataLen = dataLen;
		// TODO.. copy data if needed ...
	} else {
		// TODO Mainloop has not processsed last package !!??? -> maybe we need another  queue/buffer here....
		error++;
	}
}

// This is only called once from IRQ! Inline here only makes separate routine clearer but avoids another call stack.
static inline void processRxByteIRQ(TTC_T *pTtc, uint8_t byte) {
	switch (pTtc->RxState) {
	case TTC_RX_IDLE:
		if (byte == TTC_OP_RECEIVE) {
			pTtc->RxState = TTC_RX_RECEIVE_C;
		} else if (byte == TTC_OP_TRANSMIT) {
			pTtc->RxState = TTC_RX_TRANSMIT_C;
		} else if (byte == TTC_OP_GETTELEMETRY) {
			pTtc->RxState = TTC_RX_TELEMETRY_C;
		} else if (byte == TTC_OP_OPMODE) {
			pTtc->RxState = TTC_RX_OPMODE_C;
		} else {
			// Sequence error. this seems not to be the 'real' first byte
			// TODO!!!
			error++; 	// keep state idle for resync.
		}
		break;

	case TTC_RX_TRANSMIT_C:
		// The 'action byte' can only be ACK or NAK
		if (byte == TTC_ACTION_ACK) {
			TtcPackageReceivedIRQ(pTtc, TTC_PRID_TRANSMIT_ACK, 0, 0);
		} else if (byte == TTC_ACTION_NACK) {
			TtcPackageReceivedIRQ(pTtc, TTC_PRID_TRANSMIT_NAK, 0, 0);
		} else {
			// Sequence error. this seems not to be a valid 2nd byte for TRANSMIT answer
			// TODO
			error++;		// try to resync by setting state to idle
		}
		pTtc->RxState = TTC_RX_IDLE;
		break;

	case TTC_RX_OPMODE_C:
		pTtc->RxState = TTC_RX_IDLE;
		// The 'action byte' can be ACK or NAK (-> answer to our OPMODE command), or OPMODE 'Command initiated from STACIE' -> 4 bytes)
		if (byte == TTC_ACTION_ACK) {
			TtcPackageReceivedIRQ(pTtc, TTC_PRID_OPMODE_ACK, 0, 0);
		} else if (byte == TTC_ACTION_NACK) {
			TtcPackageReceivedIRQ(pTtc, TTC_PRID_OPMODE_NAK, 0, 0);
		} else if (byte == TTC_ACTION_EXEC) {
			pTtc->RxExpectedDataBytes = 2;		// including Checksum byte
			pTtc->RxIdx = 0;
			pTtc->RxChecksum = 0;
			pTtc->RxState = TTC_RX_OPMODE_DATA;
		} else {
			// Sequence error. this seems not to be a valid 2nd byte for OPMODE answer
			// TODO
			error++;		// try to resync by setting state to idle
		}
		break;

	case TTC_RX_OPMODE_DATA:
	case TTC_RX_RECEIVE_DATA:
	case TTC_RX_TELEMETRY_DATA:
		pTtc->RxDataBuffer[pTtc->RxIdx++] = byte;
		//pTtc->RxChecksum =
		c_CRC8(byte, &pTtc->RxChecksum);
		pTtc->RxExpectedDataBytes--;
		if (pTtc->RxExpectedDataBytes <= 0) {
			if (pTtc->RxChecksum == 0) {
				// Valid Checksum
				if (pTtc->RxState == TTC_RX_OPMODE_DATA) {
					TtcPackageReceivedIRQ(pTtc, TTC_PRID_OPMODE, pTtc->RxDataBuffer , 1);
				} else if (pTtc->RxState == TTC_RX_RECEIVE_DATA) {
					TtcPackageReceivedIRQ(pTtc, TTC_PRID_GSRECEIVED, pTtc->RxDataBuffer , TTC_PAYLOAD_FULL_SIZE);
				} else if (pTtc->RxState == TTC_RX_TELEMETRY_DATA) {
					TtcPackageReceivedIRQ(pTtc, TTC_PRID_TELEMETRY_ACK, pTtc->RxDataBuffer , pTtc->RxIdx - 2);
				}
			} else {
				// TODO Checksum error
				error++;
			}
			pTtc->RxState = TTC_RX_IDLE;
		}
		break;

	case TTC_RX_RECEIVE_C:
		if (byte == TTC_ACTION_EXEC) {
			pTtc->RxExpectedDataBytes = TTC_PAYLOAD_FULL_SIZE + 1;		// including Checksum byte
			pTtc->RxIdx = 0;
			pTtc->RxChecksum = 0;
			pTtc->RxState = TTC_RX_RECEIVE_DATA;
		} else {
			// TODO:
			// in PEG this is answered with a NAK !? we do not process it here. if needed we have to define extra 'error' packages to mainloop.....
			// There is no valid 'Receive' action other then exec !!!! -< try to resync ...
			error++;
			pTtc->RxState = TTC_RX_IDLE;	//TODO !!!???
		}
		break;

	case TTC_RX_TELEMETRY_C:
		if (byte == TTC_ACTION_ACK) {
			pTtc->RxState = TTC_RX_TELEMETRY_ID;
		} else {  // PEG: NAK with content !? and Telemetry from OBC to Stacie not implemented here !!!  Telemtry should be simplified to one single data package!
			error++;
			pTtc->RxState = TTC_RX_IDLE;
		}
		break;

	case TTC_RX_TELEMETRY_ID:
		pTtc->RxIdx = 0;
		pTtc->RxChecksum = 0;
		pTtc->RxDataBuffer[pTtc->RxIdx++] = byte;
		c_CRC8(byte, &pTtc->RxChecksum);
		if (byte == TTC_TELEMETRY_RSSI) {
			pTtc->RxExpectedDataBytes = 3;		// including Checksum byte (excluding this ID byte)
		} else {
			pTtc->RxExpectedDataBytes = 2;		// including Checksum byte (excluding this ID byte)
		}
		pTtc->RxState = TTC_RX_TELEMETRY_DATA;
		break;

	default:
		pTtc->RxState = TTC_RX_IDLE;
		break;
	}
}

STATIC INLINE void countError(uint32_t bitmask) {
	// bit 6..4 irqSource from IID register
	// bit 3..0 Error bit from Line Status register
	error++;
}

void TtcUartIrqA(LPC_USART_T *pUart){
	TtcUartIrq(&ttca);
}

void TtcUartIrqC(LPC_USART_T *pUart){
	TtcUartIrq(&ttcc);
}

void TtcUartIrq(TTC_T *pTtc){
	Chip_GPIO_SetPinOutLow(LPC_GPIO, 3, 26);

	uint32_t irqid = pTtc->pUart->IIR;					// "Interrupt Identification Register"
	uint8_t irqSource = (irqid & 0x0E)>>1;			// has 3 bit as "interrupt identification"

	if (( irqid & UART_IIR_INTSTAT_PEND ) == 0) {	// This bit - indicating any interrupt pending is low active!
		// There is an Irq pending
		uint32_t ls =  pTtc->pUart->LSR;					// check line status register (for error irq this is needed to clear the IRQ).
		if (( ls & ( UART_LSR_BI | UART_LSR_FE | UART_LSR_OE | UART_LSR_PE)) > 0 ) {
			countError((ls & ( UART_LSR_BI | UART_LSR_FE | UART_LSR_OE | UART_LSR_PE) >> 1) | (irqSource << 4));
		}

		if ( ( irqSource == 0x03 ) || (irqSource == 0x02) || (irqSource == 0x06)) {
			// This was a "Rx Fifo treshhold reached" or a "char timeout" IRQ -> Bytes are available in RX FIFO to be processsed
			bool bytesAvailable = ((ls & UART_LSR_RDR) > 0);		// This check would not be deeded - as we got the IRQ.
			while (bytesAvailable) {
				processRxByteIRQ(pTtc,pTtc->pUart->RBR);				//TODO: when should we stop to do this!? depending on error connditions.
														//      (e.g. frame error has one valid char and then corrupted content.....)
				ls =  pTtc->pUart->LSR;
				if (( ls & ( UART_LSR_BI | UART_LSR_FE | UART_LSR_OE | UART_LSR_PE)) > 0 ) {
					countError((ls & ( UART_LSR_BI | UART_LSR_FE | UART_LSR_OE | UART_LSR_PE) >> 1) | (irqSource << 4));
				}
				bytesAvailable = ((ls & UART_LSR_RDR) > 0);
			}

		} else if (irqSource == 0x01) {
			// The Tx FIFO is empty (now). Lets fill it up. It can hold up to 16 (UART_TX_FIFO_SIZE) bytes.
			// no neeed to check THRE - as we got this IRQ
			char c;
			int  i = 0;
			while( i++ < UART_TX_FIFO_SIZE) {
				if (RingBuffer_Pop( & pTtc->TxRingbuffer, &c) == 1) {
					Chip_UART_SendByte( pTtc->pUart, c);
				} else {
					// We have to stop because our tx ringbuffer is empty now.
					pTtc->TxInProgress = false;
					Chip_UART_IntDisable( pTtc->pUart, UART_IER_THREINT);
					break;
				}
			}
		} else {
			// One of the 'reserved IRQ Sources ocured!!!????
			countError(irqSource << 4);
		}
	}
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 3, 26);
}


void TtcSendBytesA(uint8_t *pBytes, int len) {
	TtcSendBytes(&ttca, pBytes, len);
}
void TtcSendBytesC(uint8_t *pBytes, int len) {
	TtcSendBytes(&ttcc, pBytes, len);
}

void TtcSendBytes(TTC_T *pTtc, uint8_t *pBytes, int len) {
	if (RingBuffer_InsertMult(&pTtc->TxRingbuffer, (void*)pBytes, len) != len) {
			// Tx Buffer is to small to hold all bytes
			//thrTxError++;
	}
	if (! (pTtc->TxInProgress)) {
		// Trigger to send the first byte and enable the TxEmptyIRQ
		char c;
		pTtc->TxInProgress = true;
		RingBuffer_Pop(&pTtc->TxRingbuffer, &c);
		Chip_UART_SendByte(pTtc->pUart, c);
		Chip_UART_IntEnable(pTtc->pUart, UART_IER_THREINT);
	}
}
