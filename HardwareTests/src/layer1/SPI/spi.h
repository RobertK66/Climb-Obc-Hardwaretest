/**
 * @file stc_spi.h
 *
 * @date Created on: 07.10.2012
 * @author Andreas Sinn <andreas.sinn@spaceteam.at>
 */

#ifndef STC_SPI_H_
#define STC_SPI_H_

// ===== INCLUDES =====

#include <chip.h>

//#include "stc_debug.h"
//#include "stc_io.h"
//#include "lpc17xx_pinsel.h"
//#include "lpc17xx_spi.h"
//#include "config.h"

// ===== DEFINES =====

// Max SPI buffer length
#define SPI_BUFFER_SIZE    16
#define SPI_MAX_JOBS 16
#define SPI_ERROR_MASK (SPI_SR_BITMASK & (SPI_SR_ROVR | SPI_SR_WCOL))

//enum
//{     // SPI sensors
//    ADXL345, L3G4200D, MS5611, ADXL375
//};

//typedef struct spi_job_s
//{
//    uint8_t *txbuffer;
//    uint8_t bytes_to_send;
//    uint8_t bytes_sent;
//    uint8_t *array_to_store;
//    uint8_t bytes_to_read;
//    uint8_t bytes_read;
//    void(*chipSelectHandler)(bool select);
//    void(*jobFinishedHandler)(uint8_t *rxdata, uint8_t rxlen)
//}
//spi_job_t;
//
//typedef struct spi_jobs_s
//{
//    spi_job_t job[16];
//    uint8_t current_job;
//    uint8_t last_job_added;
//    uint8_t jobs_pending;
//}
//spi_jobs_t;


//// ===== global variables =====
//uint8_t SPI_TX_BUF[SPI_BUFFER_SIZE];
//uint8_t SPI_RX_BUF[SPI_BUFFER_SIZE];
//// SPI_CFG_Type spiInitialization;
//
//spi_jobs_t spi_jobs;
//
//uint16_t pressure_calib[8];
//

uint8_t spi_getJobsPending(void);

// ===== DEFINITIONS =====

void spi_init(void);
void SPI_IRQHandler(void);
//bool spi_add_job( void(*chipSelectHandler)(bool select), uint8_t cmd_to_send, uint8_t bytes_to_read, uint8_t *array_to_store);
bool spi_add_job( void(*chipSelect)(bool select), uint8_t* txpTr, uint8_t bytes_to_write, uint8_t *array_to_store, uint8_t bytes_to_read );

void DumpSpiJobs(void);

//bool gyro_init(void);
//void gyro_deinit(void);
//void gyro_read_values_polling(void);
//void gyro_read_temperature_polling(void);
//void gyro_select(void) ;
//void gyro_unselect(void) ;
//
//bool hig_init(void);
//void hig_deinit(void);
//void hig_read_values_polling(void);
//void hig_select(void) ;
//void hig_unselect(void) ;
//
//bool pressure_init(void);
//void pressure_read_values_polling(void);
//void pressure_select(void) ;
//void pressure_unselect(void) ;
//

//bool hig_read_values(void);
//bool gyro_read_values(void);
//bool gyro_read_temperature(void);
//bool pressure_read_values(void);
//unsigned char crc4(uint16_t n_prom[]);

#endif /* STC_SPI_H_ */
