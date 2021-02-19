/*
 * timer.h
 *
 *  Created on: 17.11.2019
 *      Author: Robert
 */

#ifndef MOD_TIM_TIMER_H_
#define MOD_TIM_TIMER_H_

#define TIM_MAIN_TICK_MS	(1000)		// This is the 'feature define' for choosing your main loop tick time in milliseconds.

#include <stdbool.h>

// Module main API
void TimInit();						    // Module Init called once prior mainloop
bool TimMain();							// Module routine participating each mainloop.

// Module function API
void TimBlockMs(uint8_t ms);			// This really blocks. So use carefully.
bool TimWaitForFalseMs(volatile bool *flag, uint8_t ms);		// This really blocks. So use carefully.

// Module global variables
extern volatile uint32_t secondsAfterReset;

#endif /* MOD_TIM_TIMER_H_ */
