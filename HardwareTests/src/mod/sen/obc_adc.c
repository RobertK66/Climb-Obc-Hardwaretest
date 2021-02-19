#include <chip.h>
#include <stdio.h>

#include "obc_adc.h"
#include "../cli/cli.h"
#include "../tim/obc_rtc.h"
#include "../../hw_obc/obc_board.h"

#define ADC_SUPPLY_CURRENT_PIN 		23 //ok
#define ADC_SUPPLY_CURRENT_PORT 	0 //ok
#define ADC_SUPPLY_CURRENT_CH 		0 //ok

#define ADC_SUPPLY_CURRENT_SP_PIN 	24 //ok
#define ADC_SUPPLY_CURRENT_SP_PORT 	0 //ok
#define ADC_SUPPLY_CURRENT_SP_CH 	1 //ok

#define ADC_TEMPERATURE_PIN 		25 //ok
#define ADC_TEMPERATURE_PORT 		0 //ok
#define ADC_TEMPERATURE_CH 			2 //ok

bool adc_initialized;
bool adc_error_counter;


void AdcReadCmd(int argc, char *argv[]){
	read_transmit_sensors();
}

void AdcInit()
{
//	PINSEL_CFG_Type PinCfg;
//	PinCfg.OpenDrain = 0;
//	PinCfg.Pinmode = 0;
//	PinCfg.Pinnum = ADC_SUPPLY_CURRENT_PIN;
//	PinCfg.Portnum = ADC_SUPPLY_CURRENT_PORT;
//	PinCfg.Funcnum = 1;
//	PINSEL_ConfigPin(&PinCfg);

	Chip_IOCON_PinMuxSet(LPC_IOCON, ADC_SUPPLY_CURRENT_PORT, ADC_SUPPLY_CURRENT_PIN, IOCON_FUNC1 | IOCON_MODE_INACT);
	Chip_IOCON_DisableOD(LPC_IOCON, ADC_SUPPLY_CURRENT_PORT, ADC_SUPPLY_CURRENT_PIN);


//	PinCfg.OpenDrain = 0;
//	PinCfg.Pinmode = 0;
//	PinCfg.Pinnum = ADC_SUPPLY_CURRENT_SP_PIN;
//	PinCfg.Portnum = ADC_SUPPLY_CURRENT_SP_PORT;
//	PinCfg.Funcnum = 1;
//	PINSEL_ConfigPin(&PinCfg);

	Chip_IOCON_PinMuxSet(LPC_IOCON, ADC_SUPPLY_CURRENT_SP_PORT, ADC_SUPPLY_CURRENT_SP_PIN, IOCON_FUNC1 | IOCON_MODE_INACT);
	Chip_IOCON_DisableOD(LPC_IOCON, ADC_SUPPLY_CURRENT_SP_PORT, ADC_SUPPLY_CURRENT_SP_PIN);


//	PinCfg.OpenDrain = 0;
//	PinCfg.Pinmode = 0;
//	PinCfg.Pinnum = ADC_TEMPERATURE_PIN;
//	PinCfg.Portnum = ADC_TEMPERATURE_PORT;
//	PinCfg.Funcnum = 1;
//	PINSEL_ConfigPin(&PinCfg);

	Chip_IOCON_PinMuxSet(LPC_IOCON, ADC_TEMPERATURE_PORT, ADC_TEMPERATURE_PIN, IOCON_FUNC1 | IOCON_MODE_INACT);
	Chip_IOCON_DisableOD(LPC_IOCON, ADC_TEMPERATURE_PORT, ADC_TEMPERATURE_PIN);

	ADC_CLOCK_SETUP_T init;
	init.adcRate = 1000;
	init.burstMode = true;

	Chip_ADC_Init(LPC_ADC, &init);

	Chip_ADC_EnableChannel(LPC_ADC, ADC_SUPPLY_CURRENT_CH, ENABLE);
	Chip_ADC_EnableChannel(LPC_ADC, ADC_SUPPLY_CURRENT_SP_CH, ENABLE);
	Chip_ADC_EnableChannel(LPC_ADC, ADC_TEMPERATURE_CH, ENABLE);

//	ADC_ChannelCmd(LPC_ADC, ADC_SUPPLY_CURRENT_CH, ENABLE);
//	ADC_ChannelCmd(LPC_ADC, ADC_SUPPLY_CURRENT_SP_CH, ENABLE);
//	ADC_ChannelCmd(LPC_ADC, ADC_TEMPERATURE_CH, ENABLE);


	Chip_ADC_SetBurstCmd(LPC_ADC, ENABLE);
	//ADC_BurstCmd(LPC_ADC, ENABLE);
	Chip_ADC_SetStartMode(LPC_ADC, 0, 0);
	//ADC_StartCmd(LPC_ADC, ADC_START_CONTINUOUS);

	adc_initialized = 1;
	adc_error_counter = 0;

	RegisterCommand("adcRead", AdcReadCmd);

}

#define RBF_PIN			21  //ok
#define RBF_PORT		0 //ok
#define SUPPLY_RAIL_PIN 25 // ok
#define SUPPLY_RAIL_PORT 3 // ok

#define BL_SEL1_PIN 29 // ok, // usb !
#define BL_SEL1_PORT 0 // ok

void read_transmit_sensors()
{

	uint16_t adc_val; // = ADC_ChannelGetData(LPC_ADC, ADC_SUPPLY_CURRENT_CH);
	Chip_ADC_ReadValue(LPC_ADC, ADC_SUPPLY_CURRENT_CH, &adc_val);

	float val = (adc_val * (3.3/4096) - 0.01) /100/0.1;
	printf("\nCUR-MC: %.2f mA;", 1000*val);

	//adc_val = ADC_ChannelGetData(LPC_ADC, ADC_SUPPLY_CURRENT_SP_CH);
	Chip_ADC_ReadValue(LPC_ADC, ADC_SUPPLY_CURRENT_SP_CH, &adc_val);
	val = (adc_val * (3.3/4096) - 0.01) /100/0.1;
	printf("CUR-SP: %.2f mA;", 1000*val);

	//adc_val = ADC_ChannelGetData(LPC_ADC, ADC_TEMPERATURE_CH);
	Chip_ADC_ReadValue(LPC_ADC, ADC_TEMPERATURE_CH, &adc_val);
	val = 25 + (adc_val * (3.3/4096) - 0.75) / 0.01;
	printf("TMP-AN: %.3f C;", val);



	printf("RTC: %d, %d \n", rtc_get_time(), rtc_get_date());


	printf("RBF: %d, Supply-R: %c, BL: %s\n", ObcGetRbfIsInserted(),  ObcGetSupplyRail(), ObcGetBootmodeStr());
}
