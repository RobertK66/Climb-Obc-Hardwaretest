#ifndef OBC_RTC_H
#define OBC_RTC_H

/* Standard includes */
#include <stdint.h>
#include <stdlib.h>

#include <chip.h>

// Module Main API
void RtcInit(void);
void RtcMain(void);

//  Module functions API
bool RtcIsGprChecksumOk(void);
void RtcSetDate(uint16_t year, uint8_t month, uint8_t dayOfMonth);
void RtcSetTime(uint8_t hours, uint8_t minutes, uint8_t seconds, bool synchronized);
void RtcReadAllGprs(uint8_t *data);

uint32_t rtc_get_date(void);
uint32_t rtc_get_time(void);

// Usage of 19 bytes General Purpose Register
typedef enum rtc_gpridx_e {
	RTC_GPRIDX_STATUS = 0,

	RTC_GPRIDX_CRC8 = 19
} rtc_gpridx_t;


uint8_t RtcReadGpr(rtc_gpridx_t idx);
void RtcWriteGpr(rtc_gpridx_t idx, uint8_t byte);


#endif /* OBC_RTC_H */
