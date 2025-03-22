/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
#include "string.h"
#include "math.h"

#include "stdbool.h"

#include "stdlib.h"
#include "time.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

RTC_HandleTypeDef hrtc;

SD_HandleTypeDef hsd;

SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart4;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;
UART_HandleTypeDef huart6;
DMA_HandleTypeDef hdma_uart4_rx;
DMA_HandleTypeDef hdma_usart2_rx;
DMA_HandleTypeDef hdma_usart2_tx;
DMA_HandleTypeDef hdma_usart3_rx;
DMA_HandleTypeDef hdma_usart3_tx;
DMA_HandleTypeDef hdma_usart6_rx;
DMA_HandleTypeDef hdma_usart6_tx;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_UART4_Init(void);
static void MX_ADC1_Init(void);
static void MX_SDIO_SD_Init(void);
static void MX_USART6_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_RTC_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void Send_To_ESP32(void);
static int Send_To_SIM7600(void);
void Read_RK400(float ch, uint8_t proceed);
void Communication_Init(void);
void Time_Update_ESP_Helper(void);
void Wake_SIM7600(void);
void Sleep_SIM7600(void);



/* --- Makro & Konstanta --- */
#define MODBUS_BUFFER_SIZE         32
#define UART6_RX_BUFFER_SIZE       100
#define ESP_BUFFER_SIZE			   320
#define SIM_BUFFER_SIZE			   256
#define DEBOUNCE_TIME              50

#define HR65_ADDR                  0x01
#define HR65_START                 0x0001

#define RK900_ADDR                 0x08
#define RK900_START                0x0005

#define NO_CHANGE 					-1

/* Toleransi dan jumlah pembacaan stabil untuk sensor RK900 dan HR65 */
#define RK900_REQUIRED_STABLE_COUNT    3
#define RK900_TOL_WIND_SPEED           100.0f
#define RK900_TOL_WIND_DIR             360.0f
#define RK900_TOL_TEMP                 5.0f
#define RK900_TOL_HUMIDITY             5.0f
#define RK900_TOL_PRESSURE             5.5f
#define RK400_RESOLUTION			   0.2f

#define HR65_REQUIRED_STABLE_COUNT     3
#define HR65_TOL_DISTANCE              5.0f


/* --- Buffer Global --- */
static uint8_t modbus_tx_buffer[MODBUS_BUFFER_SIZE];
static uint8_t modbus_rx_buffer[MODBUS_BUFFER_SIZE];
static uint8_t uart6_rx_buffer[UART6_RX_BUFFER_SIZE];
static uint8_t esp_tx_buffer[ESP_BUFFER_SIZE];
static uint8_t esp_rx_buffer[ESP_BUFFER_SIZE];
static uint8_t sim_rx_buffer[SIM_BUFFER_SIZE];



/* --- Variabel Global (volatile) --- */
volatile float windSpeed = 0.0f;
volatile float windDir = 0.0f;
volatile float temp_c = 0.0f;
volatile float humidity_pct = 0.0f;
volatile float pressure_mbar = 0.0f;
volatile float distance = 0.0f;
volatile float curah_hujan = 0.0f;
volatile float solar_rad = 0.0f;
volatile float Latitude = 0.0f;
volatile float Longitude = 0.0f;



volatile uint8_t OK_send = 1;
volatile uint8_t OK_Update = 1;
volatile uint16_t sim_rxSize = 0;
volatile uint8_t flag_update = 0;
volatile uint8_t flag_recentval = 0;
volatile uint8_t flag_reset = 0;
volatile uint8_t flag_modbus = 0;
volatile uint8_t flag_interval = 0;
volatile uint8_t flag_com = 0;
volatile uint8_t mode_interval = 0;
volatile uint8_t mode_com = 0;
volatile uint8_t flag_apply = 0;
volatile uint8_t flag_delete = 0;
volatile uint8_t flag_time_update_esp_helper = 0;
volatile uint8_t flag_periodic_request_time_update = 0;
volatile uint8_t flag_periodic_meas = 0;
volatile uint8_t flag_tipp = 0;
volatile uint8_t flag_sleep = 1;
volatile uint8_t flag_recent_latlong = 0;
volatile uint8_t flag_recent_time = 0;
volatile uint8_t flag_timeupdate_only = 0;
volatile uint8_t flag_update_gps = 0;
volatile uint8_t flag_set_wifi = 0;
volatile uint8_t flag_ssid_info = 0;
volatile uint8_t flag_wake_print = 0;


volatile uint8_t already_on = 0;


volatile uint16_t interval = 0;

char ssid[32] = {0};
char password[32] = {0};

volatile HAL_StatusTypeDef modbus_status = HAL_ERROR;



#if 1 //NEXTION DISPLAY


/**
 * @brief Mengirim perintah string ke Nextion.
 */
static const uint8_t CMD_END[3] = {0xFF, 0xFF, 0xFF};
void NEXTION_SendString(const char *ID, const char *string) {
	char buf[50];
	int len = snprintf(buf, sizeof(buf), "%s.txt=\"%s\"", ID, string);
	if (len > 0) {
		HAL_UART_Transmit(&huart6, (uint8_t *)buf, len, 1000);
		HAL_UART_Transmit(&huart6, (uint8_t *)CMD_END, sizeof(CMD_END), 1000);
	}
}

/**
 * @brief Mengirim nilai float ke Nextion (format 1 desimal).
 */
void NEXTION_SendFloat(const char *ID, float value) {
	char str[20];
	snprintf(str, sizeof(str), "%.1f", value);
	NEXTION_SendString(ID, str);
}

/**
 * @brief Mengirim nilai integer ke Nextion.
 */
void NEXTION_SendInt(const char *ID, int value) {
	char str[10];
	snprintf(str, sizeof(str), "%d", value);
	NEXTION_SendString(ID, str);
}

#endif


#if 1 //EXTERNAL RTC
#define RTC_DS3231_ADDRESS 0xD0
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress,
		uint16_t MemAddSize, uint8_t *pData, uint16_t Size, uint32_t Timeout);
// Convert normal decimal numbers to binary coded decimal
uint8_t decToBcd(int val)
{
	return (uint8_t)( (val/10*16) + (val%10) );
}
// Convert binary coded decimal to normal decimal numbers
int bcdToDec(uint8_t val)
{
	return (int)( (val/16*10) + (val%16) );
}

typedef struct {
	uint8_t seconds;
	uint8_t minutes;
	uint8_t hour;
	uint8_t dayofweek;
	uint8_t dayofmonth;
	uint8_t month;
	uint8_t year;

}TIME_DS3231;

TIME_DS3231 rtcds32;

/* function to set time */
void Set_Time_DS3231 (uint8_t sec, uint8_t min, uint8_t hour, uint8_t dow, uint8_t dom, uint8_t month, uint16_t year)
{
	uint8_t set_time[7];
	set_time[0] = decToBcd(sec);
	set_time[1] = decToBcd(min);
	set_time[2] = decToBcd(hour);
	set_time[3] = decToBcd(dow);
	set_time[4] = decToBcd(dom);
	set_time[5] = decToBcd(month);

	if(year>2000){
		year-=2000;
		set_time[6] = decToBcd(year);
	}else{
		set_time[6] = decToBcd(year);
	}

	printf("Set time RTC DS3231 success\n");


	HAL_I2C_Mem_Write(&hi2c1, RTC_DS3231_ADDRESS, 0x00, 1, set_time, 7, 1000);
}

char time_display_buffer[30]="-";
void Get_Time_DS3231 (void)
{
	uint8_t get_time[7];
	HAL_I2C_Mem_Read(&hi2c1, RTC_DS3231_ADDRESS, 0x00, 1, get_time, 7, 1000);
	HAL_Delay(100);
	rtcds32.seconds = bcdToDec(get_time[0]);
	rtcds32.minutes = bcdToDec(get_time[1]);
	rtcds32.hour = bcdToDec(get_time[2]);
	rtcds32.dayofweek = bcdToDec(get_time[3]);
	rtcds32.dayofmonth = bcdToDec(get_time[4]);
	rtcds32.month = bcdToDec(get_time[5]);
	rtcds32.year = bcdToDec(get_time[6]);
}

#endif

#if 1 //INTERNAL RTC

RTC_HandleTypeDef hrtc;

void Set_Time_Internal_RTC (uint8_t sec, uint8_t min, uint8_t hour, uint8_t day, uint8_t date, uint8_t month, uint16_t year)
{
	if(year>2000){
		year-=2000;}

	RTC_TimeTypeDef sTime = {0};
	RTC_DateTypeDef sDate = {0};

	sTime.Hours = hour;
	sTime.Minutes = min;
	sTime.Seconds = sec;
	sDate.WeekDay = day;
	sDate.Month = month;
	sDate.Date = date;
	sDate.Year = year;
	sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	sTime.StoreOperation = RTC_STOREOPERATION_RESET;
	if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
	{
		Error_Handler();
	}
	if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
	{
		Error_Handler();
	}else{
		printf("Set time internal RTC success\n");
	}
	HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, 0x2345);



}

typedef struct {
	uint8_t seconds;
	uint8_t minutes;
	uint8_t hour;
	uint8_t dayofweek;
	uint8_t dayofmonth;
	uint8_t month;
	uint8_t year;

}TIME_STM;

TIME_STM rtcstm;

void Get_Time_Internal_RTC(void)
{
	RTC_DateTypeDef sDate;
	RTC_TimeTypeDef sTime;


	HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);


	rtcstm.seconds    = sTime.Seconds;
	rtcstm.minutes    = sTime.Minutes;
	rtcstm.hour       = sTime.Hours;
	rtcstm.dayofweek  = sDate.WeekDay;
	rtcstm.dayofmonth = sDate.Date;
	rtcstm.month      = sDate.Month;
	rtcstm.year       = sDate.Year;
}


void Set_Alarm_A_Intenal_RTC (uint8_t hr, uint8_t min, uint8_t sec, uint8_t date)
{
	RTC_AlarmTypeDef sAlarm = {0};
	sAlarm.AlarmTime.Hours = hr;
	sAlarm.AlarmTime.Minutes = min;
	sAlarm.AlarmTime.Seconds = sec;
	sAlarm.AlarmTime.SubSeconds = 0;
	sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
	sAlarm.AlarmMask = RTC_ALARMMASK_NONE;
	sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
	sAlarm.AlarmDateWeekDaySel = RTC_ALARMMASK_DATEWEEKDAY;
	sAlarm.AlarmDateWeekDay = date;
	sAlarm.Alarm = RTC_ALARM_A;
	if (HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BIN) != HAL_OK)
	{
		Error_Handler();
	}

	printf("Set alarm A berhasil: jam %d, menit %d, detik %d\n", hr, min, sec);

}

void Set_Alarm_B_Intenal_RTC (uint8_t hr, uint8_t min, uint8_t sec, uint8_t date)
{
	RTC_AlarmTypeDef sAlarm = {0};
	sAlarm.AlarmTime.Hours = hr;
	sAlarm.AlarmTime.Minutes = min;
	sAlarm.AlarmTime.Seconds = sec;
	sAlarm.AlarmTime.SubSeconds = 0;
	sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
	sAlarm.AlarmMask = RTC_ALARMMASK_NONE;
	sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
	sAlarm.AlarmDateWeekDaySel = RTC_ALARMMASK_DATEWEEKDAY;
	sAlarm.AlarmDateWeekDay = date;
	sAlarm.Alarm = RTC_ALARM_B;
	if (HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BIN) != HAL_OK)
	{
		Error_Handler();
	}

	printf("Set alarm B berhasil: jam %d, menit %d, detik %d\n", hr, min, sec);

}

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc)
{
	flag_periodic_meas = 1;
}

void HAL_RTCEx_AlarmBEventCallback(RTC_HandleTypeDef *hrtc)
{
	flag_periodic_request_time_update = 1;
	flag_update_gps = 1;
}

void Update_Time_Buffer(){
	Get_Time_Internal_RTC();
	snprintf(time_display_buffer, sizeof(time_display_buffer),
			"%02d/%02d/%04d  %02d.%02d.%02d",
			rtcstm.dayofmonth, rtcstm.month, 2000 + rtcstm.year,
			rtcstm.hour, rtcstm.minutes, rtcstm.seconds);
}

/**
 * @brief Interval Inisial
 */


// Fungsi pembantu untuk menentukan jumlah hari dalam sebulan (dengan memperhatikan tahun kabisat)
uint8_t daysInMonth(uint8_t month, uint16_t year) {
	switch (month) {
	case 1: case 3: case 5: case 7: case 8: case 10: case 12:
		return 31;
	case 4: case 6: case 9: case 11:
		return 30;
	case 2:
		// Tahun kabisat: jika tahun habis dibagi 4 dan bukan kelipatan 100, atau habis dibagi 400
		if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
			return 29;
		else
			return 28;
	default:
		return 31;
	}
}

void Initial_Interval(uint16_t intervalSeconds) {

	Get_Time_Internal_RTC();


	uint32_t currentSeconds = rtcstm.hour * 3600 + rtcstm.minutes * 60 + rtcstm.seconds;


	uint32_t remainder = currentSeconds % intervalSeconds;
	uint16_t to_time_elapsed = (remainder == 0 ? intervalSeconds : (intervalSeconds - remainder));


	uint32_t futureSeconds = currentSeconds + to_time_elapsed;

	uint8_t daysToAdd = futureSeconds / 86400;  // 86400 detik = 24 jam
	if (daysToAdd > 0) {
		futureSeconds -= 86400;   }

	uint8_t alarmHour   = futureSeconds / 3600;
	uint8_t alarmMinute = (futureSeconds % 3600) / 60;
	uint8_t alarmSecond = futureSeconds % 60;

	uint8_t alarmDayOfWeek = 1;

	// 9. Panggil fungsi untuk mengatur alarm RTC dengan parameter yang sesuai
	Set_Alarm_A_Intenal_RTC(alarmHour, alarmMinute, alarmSecond, alarmDayOfWeek);
}




#endif

/**
 * @brief Memulai ulang penerimaan DMA untuk UART tertentu.
 */
static inline void Restart_UART_DMA(UART_HandleTypeDef *huart, uint8_t *buffer, uint16_t size) {
	HAL_UARTEx_ReceiveToIdle_DMA(huart, buffer, size);
}

#if 1 //MICROSD
/**
 * @brief MICRO_SD
 */
char space_info[15];
void Get_SD_Space(void){
	FATFS FatFs;
	FRESULT FR_Status;
	FATFS *FS_Ptr;
	DWORD FreeClusters;
	uint32_t TotalSize, FreeSpace;
	//Cek Mounting
	FR_Status = f_mount(&FatFs, SDPath, 1);


	if (FR_Status == FR_OK){
		//------------------[ Get & Print The SD Card Size & Free Space ]--------------------
		f_getfree("", &FreeClusters, &FS_Ptr);
		TotalSize = (uint32_t)((FS_Ptr->n_fatent - 2) * FS_Ptr->csize * 0.5);
		FreeSpace = (uint32_t)(FreeClusters * FS_Ptr->csize * 0.5);

		float total_gb = TotalSize / (1024.0 * 1024.0);
		float free_gb = FreeSpace / (1024.0 * 1024.0);

		sprintf(space_info, "%.1f / %.1f GB", free_gb, total_gb);

	}else{
		printf("Error! Mounting SD Card, Error Code: (%i)\r\n", FR_Status);
	}

}

// Fungsi untuk menulis ke SD Card
// Perhatikan bahwa parameter curah_hujan telah ditambahkan sebelum waterl.
void Write_SD(const char *filename, uint8_t inval, uint8_t modes,
		uint8_t tgl, uint8_t bln, uint8_t th,
		uint8_t jam, uint8_t mnt, uint8_t dtk,
		float suhu, float rh, float windir, float winds,
		float pressure, float radiasi, float curah_hujan, float waterl,
		float lat, float lot, char *ssid, char *pass) {
	FATFS FatFs;
	FIL Fil;
	FRESULT FR_Status;
	UINT WWC; // Write Word Counter
	char RW_Buffer[100];
	__disable_irq();

	// Mount SD Card
	FR_Status = f_mount(&FatFs, "", 1);
	if (FR_Status != FR_OK) {
		printf("Error mounting SD card, Code: %d\r\n", FR_Status);
		return;
	}

	// Jika file yang dituju adalah Config.txt, maka file akan ditimpa dengan konfigurasi baru
	if (strcmp(filename, "Config.txt") == 0) {
		// Format konfigurasi ke dalam buffer
		snprintf(RW_Buffer, sizeof(RW_Buffer), "Interval: %u, Mode: %u", inval, modes);

		// Buka file dengan opsi membuat file baru atau menimpa file yang sudah ada
		FR_Status = f_open(&Fil, filename, FA_WRITE | FA_CREATE_ALWAYS);
		if (FR_Status == FR_OK) {
			f_write(&Fil, RW_Buffer, strlen(RW_Buffer), &WWC);
			f_close(&Fil);
		} else {
			printf("Error opening file %s, Code: %d\r\n", filename, FR_Status);
		}
	}

	// Jika file yang dituju adalah Config.txt, maka file akan ditimpa dengan konfigurasi baru
	if (strcmp(filename, "WIFICONF.txt") == 0) {
		// Format konfigurasi ke dalam buffer
		snprintf(RW_Buffer, sizeof(RW_Buffer), "SSID: %s, PASS: %s", ssid, pass);

		// Buka file dengan opsi membuat file baru atau menimpa file yang sudah ada
		FR_Status = f_open(&Fil, filename, FA_WRITE | FA_CREATE_ALWAYS);
		if (FR_Status == FR_OK) {
			f_write(&Fil, RW_Buffer, strlen(RW_Buffer), &WWC);
			f_close(&Fil);
		} else {
			printf("Error opening file %s, Code: %d\r\n", filename, FR_Status);
		}
	}

	if (strcmp(filename, "LASTVAL.txt") == 0) {
		// Buka file dengan mode write dan open-always (jika tidak ada, maka file akan dibuat)
		FR_Status = f_open(&Fil, filename, FA_WRITE | FA_CREATE_ALWAYS);
		if (FR_Status == FR_OK) {
			// Pindahkan pointer ke akhir file agar data baru ditambahkan
			f_lseek(&Fil, f_size(&Fil));

			// Jika file masih kosong, tulis header terlebih dahulu.
			if (f_size(&Fil) == 0) {
				snprintf(RW_Buffer, sizeof(RW_Buffer),
						"date,suhu,rh,winds,windir,pressure,radiasi,curah_hujan,waterl\r\n");
				f_write(&Fil, RW_Buffer, strlen(RW_Buffer), &WWC);
			}

			// Format tanggal dan data (format waktu: "dd/mm/yyyy hh.mm.ss")
			// Data: suhu, rh, winds, windir, pressure, radiasi, curah_hujan, waterl
			snprintf(RW_Buffer, sizeof(RW_Buffer),
					"%02u/%02u/20%02u  %02u.%02u.%02u, %.1f, %.1f, %.1f, %.1f, %.1f, %.1f, %.1f, %.1f\r\n",
					tgl, bln, th, jam, mnt, dtk,
					suhu, rh, winds, windir, pressure, radiasi, curah_hujan, waterl);

			// Tulis data ke file
			f_write(&Fil, RW_Buffer, strlen(RW_Buffer), &WWC);
			f_close(&Fil);

		} else {
			printf("Error opening file %s, Code: %d\r\n", filename, FR_Status);
		}
	}

	if (strcmp(filename, "LOCATION.txt") == 0) {

		snprintf(RW_Buffer, sizeof(RW_Buffer), "Latitude: %.5f, Longitude: %.5f", lat, lot);

		FR_Status = f_open(&Fil, filename, FA_WRITE | FA_CREATE_ALWAYS);
		if (FR_Status == FR_OK) {
			f_write(&Fil, RW_Buffer, strlen(RW_Buffer), &WWC);
			f_close(&Fil);
		} else {
			printf("Error opening file %s, Code: %d\r\n", filename, FR_Status);
		}
		}

	// Jika file yang dituju adalah Measured_Params.txt, maka data akan di-append
	if (strcmp(filename, "MEASURE.txt") == 0) {
		// Buka file dengan mode write dan open-always (jika tidak ada, maka file akan dibuat)
		FR_Status = f_open(&Fil, filename, FA_WRITE | FA_OPEN_ALWAYS);
		if (FR_Status == FR_OK) {
			// Pindahkan pointer ke akhir file agar data baru ditambahkan
			f_lseek(&Fil, f_size(&Fil));

			// Jika file masih kosong, tulis header terlebih dahulu.
			if (f_size(&Fil) == 0) {
				snprintf(RW_Buffer, sizeof(RW_Buffer),
						"date,suhu,rh,winds,windir,pressure,radiasi,curah_hujan,waterl\r\n");
				f_write(&Fil, RW_Buffer, strlen(RW_Buffer), &WWC);
			}

			// Format tanggal dan data (format waktu: "dd/mm/yyyy hh.mm.ss")
			// Data: suhu, rh, winds, windir, pressure, radiasi, curah_hujan, waterl
			snprintf(RW_Buffer, sizeof(RW_Buffer),
					"%02u/%02u/20%02u  %02u.%02u.%02u, %.1f, %.1f, %.1f, %.1f, %.1f, %.1f, %.1f, %.1f\r\n",
					tgl, bln, th, jam, mnt, dtk,
					suhu, rh, winds, windir, pressure, radiasi, curah_hujan, waterl);

			// Tulis data ke file
			f_write(&Fil, RW_Buffer, strlen(RW_Buffer), &WWC);
			f_close(&Fil);

		} else {
			printf("Error opening file %s, Code: %d\r\n", filename, FR_Status);
		}
	}


	__enable_irq();
}

// Fungsi untuk membaca dari SD Card
void Read_SD(const char *filename, uint8_t proceed) {
	__disable_irq();
	FATFS FatFs;
	FIL Fil;
	FRESULT FR_Status;
	UINT RWC;
	char RW_Buffer[200];

	// Mount SD Card
	FR_Status = f_mount(&FatFs, "", 1);
	if (FR_Status != FR_OK) {
		printf("Error mounting SD card, Code: %d\r\n", FR_Status);
		return;
	}

	// Baca file konfigurasi jika diperlukan
	if (strcmp(filename, "Config.txt") == 0 && proceed==1) {
		FR_Status = f_open(&Fil, filename, FA_READ);
		if (FR_Status == FR_OK) {
			if (f_read(&Fil, RW_Buffer, sizeof(RW_Buffer) - 1, &RWC) == FR_OK && RWC > 0) {
				RW_Buffer[RWC] = '\0';  // Pastikan string null-terminated

				// Parsing string konfigurasi
				unsigned int tmp_interval, tmp_mode;
				if (sscanf(RW_Buffer, "Interval: %u, Mode: %u", &tmp_interval, &tmp_mode) == 2) {
					// Misalnya, simpan ke variabel global mode_interval dan mode_com
					mode_interval = (uint8_t)tmp_interval;
					mode_com = (uint8_t)tmp_mode;
					//                    printf("Interval: %u, Mode: %u\n", mode_interval, mode_com);
				} else {
					printf("Failed to parse configuration\n");
				}
			}
			f_close(&Fil);
		} else {
			printf("Error opening file %s, Code: %d\r\n", filename, FR_Status);
		}
	}

	if (strcmp(filename, "WIFICONF.txt") == 0 && proceed == 1) {
		FR_Status = f_open(&Fil, filename, FA_READ);
		if (FR_Status == FR_OK) {
			if (f_read(&Fil, RW_Buffer, sizeof(RW_Buffer) - 1, &RWC) == FR_OK && RWC > 0) {
				RW_Buffer[RWC] = '\0';  // Pastikan string null-terminated

				char tmp_ssid[50] = {0}, tmp_pass[50] = {0};
				/*
				 * Menggunakan format specifier berikut:
				 * - %49[^,]  : membaca maksimal 49 karakter sampai bertemu koma, sehingga bisa menangani spasi dalam SSID
				 * - %49[^\n] : membaca maksimal 49 karakter sampai akhir baris, sehingga bisa menangani spasi dalam password
				 */
				if (sscanf(RW_Buffer, "SSID: %49[^,], PASS: %49[^\n]", tmp_ssid, tmp_pass) == 2) {
					strcpy(ssid, tmp_ssid);
					strcpy(password, tmp_pass);
				} else {
					printf("Failed to parse configuration\n");
				}
			} else {
				printf("Failed to read from file or file is empty\n");
			}
			f_close(&Fil);
		} else {
			printf("Error opening file %s, Code: %d\r\n", filename, FR_Status);
		}
	}


	if (strcmp(filename, "LOCATION.txt") == 0 && proceed==1) {
		FR_Status = f_open(&Fil, filename, FA_READ);
		if (FR_Status == FR_OK) {
			if (f_read(&Fil, RW_Buffer, sizeof(RW_Buffer) - 1, &RWC) == FR_OK && RWC > 0) {
				RW_Buffer[RWC] = '\0';  // Pastikan string null-terminated

				// Parsing string konfigurasi
				float tmp_lat, tmp_lot;
				if (sscanf(RW_Buffer, "Latitude: %f, Longitude: %f", &tmp_lat, &tmp_lot) == 2) {
					// Misalnya, simpan ke variabel global mode_interval dan mode_com
					Latitude = tmp_lat;
					Longitude = tmp_lot;
				} else {
					printf("Failed to parse configuration\n");
				}
			}
			f_close(&Fil);
		} else {
			printf("Error opening file %s, Code: %d\r\n", filename, FR_Status);
		}
	}

	// Baca file Measured_Params.txt
	if (strcmp(filename, "LASTVAL.txt") == 0 && proceed==1) {
		FR_Status = f_open(&Fil, filename, FA_READ);
		if (FR_Status == FR_OK) {
			char line[200];
			char lastLine[200] = {0};

			// Baca file baris per baris dan simpan baris terakhir (abaikan baris header)
			while (f_gets(line, sizeof(line), &Fil)) {
				// Jika baris diawali dengan "date", anggap itu header dan lewati
				if (strncmp(line, "date", 4) == 0) continue;
				strcpy(lastLine, line);
			}
			f_close(&Fil);

			if (strlen(lastLine) > 0) {
				char dateStr[30];
				float temp, rh, winds, windir, pressure, radiasi, rain, waterl;

				// Parsing baris terakhir.
				// Format: "dd/mm/20yy hh.mm.ss, suhu, rh, winds, windir, pressure, radiasi, curah_hujan, waterl"
				int parsed = sscanf(lastLine, " %29[^,], %f, %f, %f, %f, %f, %f, %f, %f",
						dateStr, &temp, &rh, &winds, &windir, &pressure, &radiasi, &rain, &waterl);
				if (parsed == 9) {
					// Simpan nilai hasil parsing ke variabel global
					strncpy(time_display_buffer, dateStr, sizeof(time_display_buffer));
					time_display_buffer[sizeof(time_display_buffer) - 1] = '\0'; // pastikan null-terminated
					temp_c         = temp;
					humidity_pct   = rh;
					windSpeed      = winds;
					windDir        = windir;
					pressure_mbar  = pressure;
					solar_rad      = radiasi;
					curah_hujan    = rain;
					distance       = waterl;

					// Tampilkan hasil parsing (optional/debug)
					//                    printf("Data ter-parsing:\n");
					//                    printf("Waktu         : %s\n", time_display_buffer);
					//                    printf("Suhu          : %.1f C\n", temp_c);
					//                    printf("RH            : %.1f %%\n", humidity_pct);
					//                    printf("Wind Speed    : %.1f\n", windSpeed);
					//                    printf("Wind Direction: %.1f\n", windDir);
					//                    printf("Pressure      : %.1f mbar\n", pressure_mbar);
					//                    printf("Solar Radiasi : %.1f\n", solar_rad);
					//                    printf("Curah Hujan   : %.1f\n", curah_hujan);
					//                    printf("Water Level   : %.1f\n", distance);
				} else {
					printf("Gagal parsing data, hanya ter-parsing %d nilai\n", parsed);
				}
			} else {
				printf("Tidak ada data dalam file %s\n", filename);
			}
		} else {
			printf("Error membuka file %s untuk membaca, Code: %d\r\n", filename, FR_Status);
		}
	}
	__enable_irq();
}




/**
 * @brief DELETE FILE
 */
void Delete_File(const char *filename) {

	FATFS FatFs;    // Struktur file system
	FRESULT FR_Status;
	FILINFO fno;
	__disable_irq();

	// Mount SD Card
	FR_Status = f_mount(&FatFs, "", 1);
	if (FR_Status != FR_OK) {
		printf("Error mounting SD card, Code: %d\r\n", FR_Status);
		return;
	}

	if (FR_Status != FR_OK) {
		printf("Failed to delete file %s, Code: %d\r\n", filename, FR_Status);
	}
	FR_Status = f_stat(filename, &fno);
	if (FR_Status == FR_NO_FILE) {
		printf("File %s not found, cannot delete.\r\n", filename);
	}else if (FR_Status == FR_OK) {
		FR_Status = f_unlink(filename);
	}else{
		printf("Failed to delete file %s, Code: %d\r\n", filename, FR_Status);

	}
	__enable_irq();


	flag_delete=0;

}

#endif



/**
 * @brief Set Interval
 */
void Set_Interval(uint8_t proceed){
	Read_SD("Config.txt",  proceed);
	switch (mode_interval) {
	case 1: interval = 60; break;
	case 2: interval = 300; break;
	case 3: interval = 600; break;
	case 4: interval = 1800; break;
	default: break;
	}
}

void Set_Comm(uint8_t proceed){
	Read_SD("Config.txt",  proceed);
}


#if 1 //MODBUS
/**
 * @brief Menghitung CRC–16 untuk protokol Modbus.
 */
uint16_t modbus_crc(const uint8_t *data, uint16_t length) {
	uint16_t crc = 0xFFFF;
	for (uint16_t pos = 0; pos < length; pos++) {
		crc ^= data[pos];
		for (uint8_t i = 0; i < 8; i++) {
			if (crc & 0x0001) {
				crc >>= 1;
				crc ^= 0xA001;
			} else {
				crc >>= 1;
			}
		}
	}
	return crc;
}

/**
 * @brief Parsing nilai 16–bit dari buffer (big–endian).
 */
static inline uint16_t parse_uint16_be(const uint8_t *buf, uint8_t index) {
	return ((uint16_t)buf[index] << 8) | buf[index + 1];
}
/**
 * @brief Mengirim perintah Modbus ke sensor.
 *
 * Fungsi ini membangun frame modbus, menghitung CRC, dan mengatur pin RS485.
 */
void modbus_send_command(uint8_t slave_addr, uint8_t function_code, uint16_t reg_addr, uint16_t reg_count) {
	uint8_t delay_485 = 0;
	/* Tentukan delay berdasarkan alamat slave */
	switch (slave_addr) {
	case RK900_ADDR:
		delay_485 = 50;
		break;
	case HR65_ADDR:
		delay_485 = 0;
		break;
	default:
		break;
	}

	modbus_tx_buffer[0] = slave_addr;
	modbus_tx_buffer[1] = function_code;
	modbus_tx_buffer[2] = (uint8_t)(reg_addr >> 8);
	modbus_tx_buffer[3] = (uint8_t)(reg_addr & 0xFF);
	modbus_tx_buffer[4] = (uint8_t)(reg_count >> 8);
	modbus_tx_buffer[5] = (uint8_t)(reg_count & 0xFF);

	uint16_t crc = modbus_crc(modbus_tx_buffer, 6);
	modbus_tx_buffer[6] = (uint8_t)(crc & 0xFF);
	modbus_tx_buffer[7] = (uint8_t)(crc >> 8);

	/* Aktifkan mode transmisi RS485 */
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8, GPIO_PIN_SET);
	HAL_UART_Transmit(&huart3, modbus_tx_buffer, 8, 100);
	HAL_Delay(delay_485);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8, GPIO_PIN_RESET);
}

/**
 * @brief Membaca data sensor RK900 dengan metode pembacaan stabil.
 */
volatile uint8_t RK900_Valid = 0;
void Read_RK900(void) {
	printf("====== Progress: Read RK900 (Stable Value) ======\n");

	uint8_t attempt = 0;
	uint8_t stable_count = 0;

	float prev_windSpeed = 0.0f, prev_windDir = 0.0f, prev_temp = 0.0f, prev_humidity = 0.0f, prev_pressure = 0.0f;
	float curr_windSpeed = 0.0f, curr_windDir = 0.0f, curr_temp = 0.0f, curr_humidity = 0.0f, curr_pressure = 0.0f;

	RK900_Valid = 0;

	for (attempt = 0; attempt < 10; attempt++) {
		modbus_send_command(RK900_ADDR, 0x03, 0x0000, RK900_START);
		HAL_Delay(100);

		if (modbus_rx_buffer[0] == RK900_ADDR) {
			/* Parsing nilai sensor menggunakan helper parse_uint16_be() */
			curr_windSpeed = parse_uint16_be(modbus_rx_buffer, 3) / 100.0f;
			curr_windDir   = parse_uint16_be(modbus_rx_buffer, 5) / 10.0f;
			curr_temp      = parse_uint16_be(modbus_rx_buffer, 7) / 10.0f;
			curr_humidity  = parse_uint16_be(modbus_rx_buffer, 9) / 10.0f;
			curr_pressure  = parse_uint16_be(modbus_rx_buffer, 11) / 10.0f;

			if (stable_count == 0) {
				prev_windSpeed = curr_windSpeed;
				prev_windDir   = curr_windDir;
				prev_temp      = curr_temp;
				prev_humidity  = curr_humidity;
				prev_pressure  = curr_pressure;
				stable_count = 1;
			} else {
				if ((fabs(curr_windSpeed - prev_windSpeed) < RK900_TOL_WIND_SPEED) &&
						(fabs(curr_windDir - prev_windDir)     < RK900_TOL_WIND_DIR)   &&
						(fabs(curr_temp - prev_temp)           < RK900_TOL_TEMP)       &&
						(fabs(curr_humidity - prev_humidity)   < RK900_TOL_HUMIDITY)   &&
						(fabs(curr_pressure - prev_pressure)   < RK900_TOL_PRESSURE)) {
					stable_count++;
				} else {
					stable_count = 1;
				}
				prev_windSpeed = curr_windSpeed;
				prev_windDir   = curr_windDir;
				prev_temp      = curr_temp;
				prev_humidity  = curr_humidity;
				prev_pressure  = curr_pressure;
			}

			if (stable_count >= RK900_REQUIRED_STABLE_COUNT) {
				windSpeed    = curr_windSpeed;
				windDir      = curr_windDir;
				temp_c       = curr_temp;
				humidity_pct = curr_humidity;
				pressure_mbar= curr_pressure;
				RK900_Valid  = 1;
				break;
			}
		} else {
			windSpeed    = 9999.9;
			windDir      = 9999.9;
			temp_c       = 9999.9;
			humidity_pct = 9999.9;
			pressure_mbar= 9999.9;
			RK900_Valid = 0;
			stable_count = 0;
		}
		/* Restart penerimaan DMA untuk modbus */
		Restart_UART_DMA(&huart3, modbus_rx_buffer, MODBUS_BUFFER_SIZE);
		HAL_Delay(10);
	}

	printf(RK900_Valid ? "RK900 Success\n" : "RK900 Fail\n");

	modbus_status = HAL_ERROR;
}

/**
 * @brief Membaca data sensor HR65 dengan metode pembacaan stabil.
 */
volatile uint8_t HR65_Valid = 0;
void Read_HR65(void) {
	printf("====== Progress: Read HR65 (Stable Value) ======\n");

	uint8_t attempt = 0;
	uint8_t stable_count = 0;
	float previous_distance = 0.0f, current_distance = 0.0f;

	HR65_Valid = 0;

	for (attempt = 0; attempt < 10; attempt++) {
		modbus_send_command(HR65_ADDR, 0x03, HR65_START, 0x0001);
		HAL_Delay(50);

		if (modbus_rx_buffer[0] == HR65_ADDR) {
			current_distance = parse_uint16_be(modbus_rx_buffer, 3) / 10.0f;

			if (attempt == 0) {
				previous_distance = current_distance;
				stable_count = 1;
			} else {
				if (fabs(current_distance - previous_distance) < HR65_TOL_DISTANCE) {
					stable_count++;
				} else {
					stable_count = 1;
				}
				previous_distance = current_distance;
			}

			if (stable_count >= HR65_REQUIRED_STABLE_COUNT) {
				distance = current_distance;
				HR65_Valid = 1;
				break;
			}
		} else {
			distance = 9999.9;
			stable_count = 0;
			HR65_Valid = 0;
		}
		Restart_UART_DMA(&huart3, modbus_rx_buffer, MODBUS_BUFFER_SIZE);
		HAL_Delay(10);
	}

	printf(HR65_Valid ? "HR65 Success\n" : "HR65 Fail\n");
	modbus_status = HAL_ERROR;
}

/**
 * @brief Membaca data sensor  PYR20
 */
volatile uint8_t PYR20_Valid=1;
void Read_PYR20(void) {
	// Inisialisasi srand hanya sekali, lebih baik di main() atau inisialisasi
	static int initialized = 0;
	if (!initialized) {
		srand(time(NULL));  // Inisialisasi seed hanya sekali
		initialized = 1;
	}

	solar_rad = ((rand() % 16001) + 4000) / 10.0;  // Menghasilkan angka acak dalam range 400.0 - 2000.0
}


/**
 * @brief Membaca data sensor  SHT20
 */
void Read_SHT20(void) {
	printf("====== Progress: Read RK900 (Stable Value) ======\n");


	float  curr_temp = 0.0f, curr_humidity = 0.0f;

	RK900_Valid = 0;

	modbus_send_command(0x01, 0x04, 0x0001, 0x0002);
	HAL_Delay(100);

	if (modbus_rx_buffer[0] == 0x01) {
		/* Parsing nilai sensor menggunakan helper parse_uint16_be() */
		curr_temp      = parse_uint16_be(modbus_rx_buffer, 3) / 10.0f;
		curr_humidity  = parse_uint16_be(modbus_rx_buffer, 5) / 10.0f;
		temp_c       = curr_temp;
		humidity_pct  = curr_humidity;
		RK900_Valid = 1;

	} else {
		temp_c       = 9999.9;
		humidity_pct = 9999.9;
		RK900_Valid = 0;
	}
	/* Restart penerimaan DMA untuk modbus */
	Restart_UART_DMA(&huart3, modbus_rx_buffer, MODBUS_BUFFER_SIZE);
	HAL_Delay(10);

	printf(RK900_Valid ? "SHT20 Success\n" : "SHT20 Fail\n");

	modbus_status = HAL_ERROR;
}

#endif

/**
 * @brief Menghitung curah hujan dari jumlah “tip count�? dan menampilkan hasil ke Nextion.
 */
void Read_RK400(float ch, uint8_t proceed) {
	if (proceed) {
		NEXTION_SendFloat("ch0", ch);
	}
	printf("RK400 Success\n");
}

/**
 * @brief Menampilkan nilai interval yang dipilih ke Nextion.
 */
void Capture_Choosed_Interval(uint8_t flag, uint8_t proceed) {
	uint16_t Capture_Interval = 0;
	if (flag != 0) {
		switch (flag) {
		case 1: Capture_Interval = 60; break;
		case 2: Capture_Interval = 300; break;
		case 3: Capture_Interval = 600; break;
		case 4: Capture_Interval = 1800; break;
		default: break;
		}
		if (proceed) {
			NEXTION_SendInt("interval0", Capture_Interval);
		}
	}
	flag_interval = 0;
}

/**
 * @brief Menampilkan mode komunikasi yang dipilih ke Nextion.
 */
void Capture_Choosed_Comm(uint8_t flag, uint8_t proceed) {
	const char *Capture_Mode = NULL;
	if (flag != 0) {
		switch (flag) {
		case 1: Capture_Mode = "Wi-Fi"; break;
		case 2: Capture_Mode = "LTE"; break;
		default: break;
		}
		if (proceed && Capture_Mode != NULL) {
			NEXTION_SendString("com0", Capture_Mode);
		}
	}
	flag_com = 0;
}

/**
 * @brief Menerapkan setting interval dan mode komunikasi.
 */

void Set_Apply(uint8_t apply_1, uint8_t inval, uint8_t comm) {
	// Cek apakah pengaturan harus diaplikasikan
	if (apply_1 == 1) {
		// Jika nilai interval tidak nol, perbarui interval
		if(inval != 0 && comm != 0){
			Write_SD("Config.txt", inval, comm, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,"","");
			HAL_Delay(100);
			printf("Set Interval and Communication Method Success Success\n");
		} else if (inval != 0) {
			// Hitung interval dalam detik (pastikan satuan konsisten)
			printf("Set Interval Success\n");
			// Panggil Write_SD dengan nilai interval yang baru dan NO_CHANGE untuk parameter komunikasi
			Write_SD("Config.txt", inval, NO_CHANGE, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, "", "");
			HAL_Delay(100);
		}else if (comm != 0) {
			printf("Set Communication Method Success\n");
			// Panggil Write_SD dengan NO_CHANGE untuk interval dan nilai komunikasi yang baru
			Write_SD("Config.txt", NO_CHANGE, comm, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, "","");
			HAL_Delay(100);
		}
	}

	Communication_Init();
	Set_Interval(OK_send);
	Set_Comm(OK_send);
	Initial_Interval(interval);

	// Reset flag_apply
	flag_apply = 0;
}


/**
 * @brief Mengirim seluruh nilai terkini ke Nextion.
 */
void Send_Recent_Val(void) {

	Read_SD("Config.txt", OK_send);
	switch (mode_interval) {
	case 1: NEXTION_SendString("interval0", "60"); break;
	case 2: NEXTION_SendString("interval0", "300"); break;
	case 3: NEXTION_SendString("interval0", "600"); break;
	case 4: NEXTION_SendString("interval0", "1800"); break;
	default: break;
	}
	switch (mode_com) {
	case 1: NEXTION_SendString("com0", "Wi-Fi"); break;
	case 2: NEXTION_SendString("com0", "LTE"); break;
	default: break;
	}

	NEXTION_SendString("micsd0", space_info);

	NEXTION_SendString("t0", "Pembaruan Terakhir");
	NEXTION_SendString("time0", time_display_buffer);
	NEXTION_SendFloat("ch0", curah_hujan);
	NEXTION_SendFloat("winds0", windSpeed);
	NEXTION_SendFloat("windd0", windDir);
	NEXTION_SendFloat("suhu0", temp_c);
	NEXTION_SendFloat("rh0", humidity_pct);
	NEXTION_SendFloat("press0", pressure_mbar);
	NEXTION_SendFloat("water0", distance);
	NEXTION_SendFloat("sunrad0", solar_rad);


	printf("Send Recent Val Success\n");
	flag_recentval = 0;
}

/**
 * @brief Mengirim seluruh nilai saat dinyalakan.
 */
void Send_Initial_Val(void) {


	//Config
	Get_SD_Space();
	NEXTION_SendString("micsd0", space_info);
	Read_SD("Config.txt", OK_send);

	switch (mode_interval) {
	case 1: NEXTION_SendString("interval0", "60"); break;
	case 2: NEXTION_SendString("interval0", "300"); break;
	case 3: NEXTION_SendString("interval0", "600"); break;
	case 4: NEXTION_SendString("interval0", "1800"); break;
	default: break;
	}
	switch (mode_com) {
	case 1: NEXTION_SendString("com0", "Wi-Fi"); break;
	case 2: NEXTION_SendString("com0", "LTE"); break;
	default: break;
	}
	Read_SD("LASTVAL.txt", OK_send);
	HAL_Delay(100);

	if(already_on){
		NEXTION_SendString("t0", "Pembaruan Terakhir");
		NEXTION_SendString("time0", time_display_buffer);
	}else{
		NEXTION_SendString("t0", "Tunggu Sebentar..");
		NEXTION_SendString("time0", "");
	}

	NEXTION_SendFloat("ch0", curah_hujan);
	NEXTION_SendFloat("winds0", windSpeed);
	NEXTION_SendFloat("windd0", windDir);
	NEXTION_SendFloat("suhu0", temp_c);
	NEXTION_SendFloat("rh0", humidity_pct);
	NEXTION_SendFloat("press0", pressure_mbar);
	NEXTION_SendFloat("water0", distance);
	NEXTION_SendFloat("sunrad0", solar_rad);

	Read_SD("LOCATION.txt", OK_send);

	Read_SD("WIFICONF.txt", OK_send);
	HAL_Delay(100);



	printf("Send Recent Val Success\n");
	flag_recentval = 0;
}

/**
 * @brief Meng-update nilai sensor dan menampilkan hasil ke Nextion.
 */
void Send_Update_Val(uint8_t Acc) {

	if(Acc){
		flag_periodic_meas = 0;
		OK_send = 0;

		NEXTION_SendString("time0", "Memuat Pembaruan..");

		Read_RK400(curah_hujan, 1);
		Read_HR65();
		Read_RK900();
		Read_SHT20();
		Read_PYR20();

		Read_SD("LOCATION.txt", 1);

		Read_SD("WIFICONF.txt", 1);


		RK900_Valid = 1;
		if (RK900_Valid) {
			NEXTION_SendFloat("winds0", windSpeed);
			NEXTION_SendFloat("windd0", windDir);
			NEXTION_SendFloat("suhu0", temp_c);
			NEXTION_SendFloat("rh0", humidity_pct);
			NEXTION_SendFloat("press0", pressure_mbar);
		} else {
			NEXTION_SendString("winds0", "null");
			NEXTION_SendString("windd0", "null");
			NEXTION_SendString("suhu0", "null");
			NEXTION_SendString("rh0", "null");
			NEXTION_SendString("press0", "null");
		}


		distance = 9999.9;
		HR65_Valid =1;

		if (HR65_Valid) {
			NEXTION_SendFloat("water0", distance);
		} else {
			NEXTION_SendString("water0", "null");
		}
		if(PYR20_Valid){
			NEXTION_SendFloat("sunrad0", solar_rad);
		}else{
			NEXTION_SendString("sunrad0", "null");
		}


		Update_Time_Buffer();

		Write_SD("MEASURE.txt", 1, 1, rtcstm.dayofmonth, rtcstm.month,
				rtcstm.year, rtcstm.hour, rtcstm.minutes, rtcstm.seconds,
				temp_c, humidity_pct, windDir, windSpeed, pressure_mbar, solar_rad,curah_hujan,
				distance,1,1,"","");

		Write_SD("LASTVAL.txt", 1, 1, rtcstm.dayofmonth, rtcstm.month,
				rtcstm.year, rtcstm.hour, rtcstm.minutes, rtcstm.seconds,
				temp_c, humidity_pct, windDir, windSpeed, pressure_mbar, solar_rad,curah_hujan,
				distance,1,1,"","");



		if(mode_com==1){
			NEXTION_SendString("time0", "Mengirim Data - Wi-Fi");
			printf("Sending via Wi-Fi\n");
			Send_To_ESP32();
		} else if(mode_com==2){
			NEXTION_SendString("time0", "Mengirim Data - LTE");
			printf("Sending via Selular\n");
			for(int i=0;i<2;i++){
				if(Send_To_SIM7600()){
					printf("Send via Selular: Sukses\n");
					break;
				}else{
					printf("Send via Selular: Gagal\n");
				}
				HAL_Delay(100);
			}


		}

		NEXTION_SendString("t0", "Pembaruan Terakhir");
		NEXTION_SendString("time0", time_display_buffer);


		printf("Send Update Success\n");

		OK_send = 1;
	}

}


void Send_Recent_Time(uint8_t proceed){
	char recent_time[30]="-";
	char recent_date[30]="-";

	if(proceed){

		Get_Time_Internal_RTC();
		snprintf(recent_time, sizeof(recent_time),
				"%02d:%02d:%02d",rtcstm.hour, rtcstm.minutes, rtcstm.seconds);

		snprintf(recent_date, sizeof(recent_date),
				"%02d/%02d/%04d",
				rtcstm.dayofmonth, rtcstm.month, 2000 + rtcstm.year);

		NEXTION_SendString("waktu0", recent_time);
		NEXTION_SendString("tgl0", recent_date);

		printf("Time disp\n");

		HAL_Delay(200);
	}
}

void Send_Recent_Latlong(void){
	char recent_lat[30]="-";
	char recent_long[30]="-";
	Read_SD("LOCATION.txt", OK_send);

	OK_send = 0;

	snprintf(recent_lat, sizeof(recent_lat), "%.5f", Latitude);
	snprintf(recent_long, sizeof(recent_long), "%.5f", Longitude);

	NEXTION_SendString("lat0", recent_lat);
	NEXTION_SendString("long0", recent_long);

	flag_recent_latlong = 0;

	printf("Send Recent Latlong Success\n");

	OK_send = 1;

}



#if 1 //INTERRUPT
/**
 * @brief Callback untuk eksternal interrupt (misalnya sensor hujan) dengan debounce.
 */

volatile uint32_t tip_count = 0;
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	curah_hujan+=0.2;
	flag_tipp = 1;
}



/**
 * @brief Callback untuk event penerimaan data via UART.
 *
 * Proses parsing perintah yang diterima dari Nextion dan sensor modbus.
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
	if (huart->Instance == USART2) {

		if(esp_rx_buffer[0] == 'x'){
			flag_time_update_esp_helper = 1;
		}

	}
	if (huart->Instance == USART3) {
		flag_modbus = 1;
	}
	if (huart->Instance == UART4) {
		if(Size < SIM_BUFFER_SIZE) {
			sim_rx_buffer[Size] = '\0';
		}


		Restart_UART_DMA(huart, sim_rx_buffer, SIM_BUFFER_SIZE);
	}


	if (huart->Instance == USART6) {
		uint8_t cmd1 = uart6_rx_buffer[1];
		uint8_t cmd2 = uart6_rx_buffer[2];

		if (cmd1 == 0x32) {
			switch (cmd2) {
			case 0x08:
				flag_apply = 1;
				flag_recentval = 1;
				break;
			case 0x0B:
				flag_recentval = 1;
				flag_reset = 1;
				break;
			case 0x0A:
				flag_delete = 1;
				break;
			case 0x02:
				flag_interval = 1;
				mode_interval = 1;
				break;
			case 0x03:
				flag_interval = 1;
				mode_interval = 2;
				break;
			case 0x04:
				flag_interval = 1;
				mode_interval = 3;
				break;
			case 0x05:
				flag_interval = 1;
				mode_interval = 4;
				break;
			case 0x06:
				flag_com = 1;
				mode_com = 1;
				break;
			case 0x07:
				flag_com = 1;
				mode_com = 2;
				break;
			default:
				break;
			}
		} else if (cmd1 == 0x30 && cmd2 == 0x05) {
			flag_update = 1;
		} else if(cmd1 == 0x33){
			switch(cmd2){
			case 0x05:
				//timeupdateonly
				flag_timeupdate_only=1;
				break;
			case 0x0A:
				//updategps
				flag_update_gps = 1;
				break;
			default:
				break;
			}

		}

		if(cmd1 == 0x25 && (cmd2 == 0x32 || cmd2 == 0x31 || cmd2 == 0x30 )){
			flag_recentval = 1;
			flag_recent_time = 0;

		}else if(cmd1 == 0x25 && (cmd2 == 0x33 || cmd2 == 0x74)){
			flag_sleep = 0;
			flag_recent_time = 1;
			flag_recent_latlong=1;
			flag_ssid_info = 1;
		}



		if (memchr(uart6_rx_buffer, 0x86, sizeof(uart6_rx_buffer)) != NULL) {
			//nextion sleep
			flag_sleep = 1;

		} else if(memchr(uart6_rx_buffer, 0x87, sizeof(uart6_rx_buffer)) != NULL){
			//nextion wake up
			flag_sleep = 0;
			flag_recent_latlong=1;
			flag_ssid_info = 1;
			flag_recentval = 1;
			flag_wake_print = 1;

		} else if(strstr((char*)uart6_rx_buffer, "@@") != NULL){
			flag_set_wifi = 1;

		}


		Restart_UART_DMA(&huart6, uart6_rx_buffer, UART6_RX_BUFFER_SIZE);
	}
}

#endif
/**
 * @brief RESET PENGATURAN SISTEM
 */
void Set_Config_Default(){
	Set_Apply(1, 1, 1);
	HAL_Delay(100);
	flag_reset=0;
	NVIC_SystemReset();

}

/* Fungsi _write agar printf dapat mengirim data lewat UART */
int _write(int file, char *data, int len) {
	HAL_UART_Transmit(&huart1, (uint8_t*)data, len, HAL_MAX_DELAY);
	return len;
}


#if 1 //ALL ABOUT ESP32
void ESP_INIT(){
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
}


void Send_To_ESP32(){


	snprintf((char*)esp_tx_buffer, ESP_BUFFER_SIZE,
			"ssid:%s,pass:%s,waktu:%s,latitude:%.5f,longitude:%.5f,suhu:%.1f,kelembaban:%.1f,arah_angin:%.1f,"
			"kecepatan_angin:%.1f,tekanan_udara:%.1f,radiasi_matahari:%.1f,curah_hujan:%.1f,"
			"water_level:%.1f",ssid,password,
			time_display_buffer, Latitude, Longitude, temp_c, humidity_pct, windDir,
			windSpeed, pressure_mbar, solar_rad, curah_hujan, distance);


	// Mengaktifkan sinyal wake-up jika diperlukan
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
	HAL_Delay(100);

	// Perbaikan: Mengirim data dengan panjang yang sebenarnya
	HAL_UART_Transmit(&huart2, esp_tx_buffer, strlen((char*)esp_tx_buffer), 1000);
	HAL_Delay(100);

	// Mengembalikan status pin wake-up ke semula
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
}

void Request_Update_Time_From_ESP32(){
	OK_Update = 0;
	OK_send = 0;
	NEXTION_SendString("waktu0", "Tunggu....");
	NEXTION_SendString("tgl0", "Tunggu....");

	Restart_UART_DMA(&huart2, esp_rx_buffer, ESP_BUFFER_SIZE);
	sprintf((char*)esp_tx_buffer, "timeupdate,ssid:%s,pass:%s", ssid, password);
	// Mengaktifkan sinyal wake-up jika diperlukan
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
	HAL_Delay(100);
	HAL_UART_Transmit(&huart2, esp_tx_buffer, strlen((char*)esp_tx_buffer), 100);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
	OK_Update = 1;
	OK_send = 1;

}

void Time_Update_ESP_Helper(){
	OK_Update = 0;
	uint8_t tgl, bulan, jam, menit, detik;
	uint16_t tahun;


	if(mode_com==1){
		tgl   = (esp_rx_buffer[1]  - '0') * 10 + (esp_rx_buffer[2]  - '0');
		bulan = (esp_rx_buffer[4]  - '0') * 10 + (esp_rx_buffer[5]  - '0');
		tahun = (esp_rx_buffer[7]  - '0') * 1000 + (esp_rx_buffer[8] - '0') * 100
				+ (esp_rx_buffer[9]  - '0') * 10   + (esp_rx_buffer[10] - '0');
		jam   = (esp_rx_buffer[12] - '0') * 10 + (esp_rx_buffer[13] - '0');
		menit = (esp_rx_buffer[15] - '0') * 10 + (esp_rx_buffer[16] - '0');
		detik = (esp_rx_buffer[18] - '0') * 10 + (esp_rx_buffer[19] - '0') + 2;
		Restart_UART_DMA(&huart2, esp_rx_buffer, ESP_BUFFER_SIZE);
	}

	printf("Tanggal: %02d-%02d-%04d\n", tgl, bulan, tahun);
	printf("Waktu: %02d:%02d:%02d\n", jam, menit, detik);

	Set_Time_DS3231(detik, menit, jam, 1, tgl, bulan, tahun);
	Set_Time_Internal_RTC(detik, menit, jam, 1, tgl, bulan, tahun);

	Send_Recent_Time(OK_send);


	flag_time_update_esp_helper = 0;
	OK_Update = 1;

}

void Send_Recent_SSID(void){
	OK_send = 0;
	Read_SD("WIFICONF.txt", 1);

	NEXTION_SendString("ssid0", ssid);

	char maskedPassword[64];
	snprintf(maskedPassword, sizeof(maskedPassword), "%c***%c", password[0], password[3]);
	NEXTION_SendString("sandi0", maskedPassword);

	flag_ssid_info = 0;

	printf("Send Recent SSID Success\n");



	OK_send =1;
}

void Parsing_Wifi_Credentials(){
    char *start, *end;
    char delimiter = (char)0xFF;

	uint32_t begin = HAL_GetTick();
	while (HAL_GetTick() - begin < 5000) {
		if (strstr((char*)uart6_rx_buffer, "ssid0") != NULL &&
				strstr((char*)uart6_rx_buffer, "pass0") != NULL) {
			break;
		}
	}
    // Parsing SSID: ambil substring setelah "ssid0" hingga karakter 'ÿ'
    start = strstr((char*)uart6_rx_buffer, "ssid0");
    if(start != NULL)
    {
        start += strlen("ssid0");  // Pindah pointer setelah "ssid0"
        end = strchr(start, delimiter);  // Cari batas "ÿ"
        if(end != NULL)
        {
            size_t len = end - start;
            if(len < sizeof(ssid))
            {
                strncpy(ssid, start, len);
                ssid[len] = '\0'; // Tambahkan null-terminator
            }
        }
    }else{

    	printf("SSID Null\n");

    }

    // Parsing password: ambil substring setelah "pass0" hingga karakter 'ÿ'
    start = strstr((char*)uart6_rx_buffer, "pass0");
    if(start != NULL)
    {
        start += strlen("pass0");  // Pindah pointer setelah "pass0"
        end = strchr(start, delimiter);  // Cari batas "ÿ"
        if(end != NULL)
        {
            size_t len = end - start;
            if(len < sizeof(password))
            {
                strncpy(password, start, len);
                password[len] = '\0'; // Tambahkan null-terminator
            }
        }
    }else{
    	printf("Password Null\n");
    }

    Write_SD("WIFICONF.txt", 1, 1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,ssid,password);


    printf("SSID: %s\r\n", ssid);
    printf("Password: %s\r\n", password);

    Send_Recent_SSID();


    flag_set_wifi = 0;

}




#endif

#if 1 //ALL ABOUT SIM7600


#define MAX_RETRIES     3


static void send_command(const char* cmd) {
	HAL_UART_DMAStop(&huart4);
	memset(sim_rx_buffer, 0, SIM_BUFFER_SIZE);
	Restart_UART_DMA(&huart4, sim_rx_buffer, SIM_BUFFER_SIZE);
	HAL_UART_Transmit(&huart4, (uint8_t*)cmd, strlen(cmd), HAL_MAX_DELAY);
}


static int wait_for(const char* expected, uint32_t timeout) {
	uint32_t start = HAL_GetTick();
	while (HAL_GetTick() - start < timeout) {
		// Periksa apakah respons yang diharapkan muncul dalam buffer
		if (strstr((char*)sim_rx_buffer, expected) != NULL) {
			printf("Sukses\n");
			return 1;
		}
	}
	printf("Error: Tidak menemukan '%s'. Response: %s\n", expected, sim_rx_buffer);
	return 0;
}

static int wait_for_extended(const char* expected1, const char* expected2, const char* not_expected, uint32_t timeout) {
	uint32_t start = HAL_GetTick();

	while (HAL_GetTick() - start < timeout) {
		// Periksa apakah respons yang diharapkan muncul dalam buffer
		HAL_Delay(100);
		if ((strstr((char*)sim_rx_buffer, expected1) != NULL || strstr((char*)sim_rx_buffer, expected2) != NULL) &&
		    strstr((char*)sim_rx_buffer, not_expected) == NULL) {
		    printf("Sukses\n");
		    printf("Response: %s\n", sim_rx_buffer);
		    return 1;
		}else if(strstr((char*)sim_rx_buffer, not_expected) == NULL){
			printf("Error!! Response: %s\n", sim_rx_buffer);
			return 0;
		}

	}
	printf("Error!! Response: %s\n", sim_rx_buffer);
	return 0;
}

// Fungsi untuk mengirim data MQTT (untuk topik dan payload) dengan mekanisme retry
static int send_mqtt_data(const char* cmd, const char* data, int len) {
	char full_cmd[50];
	sprintf(full_cmd, "%s\r\n", cmd);
	int retries = 0;
	while (retries < MAX_RETRIES) {
		send_command(full_cmd);
		if (wait_for(">", 5000)) {
			HAL_UART_Transmit(&huart4, (uint8_t*)data, len, HAL_MAX_DELAY);
			if (wait_for("OK", 5000))
				return 1;
		}
		retries++;
		printf("Retrying command: %s (attempt %d)\n", cmd, retries + 1);
	}
	return 0;
}

// Fungsi untuk mengirim perintah AT dan menunggu respons dengan mekanisme retry
static int send_command_with_retry(const char* cmd, const char* expected, uint32_t timeout) {
	int retries = 0;
	while (retries < MAX_RETRIES) {
		send_command(cmd);
		if (wait_for(expected, timeout))
			return 1;
		retries++;
		printf("Retrying command: %s (attempt %d)\n", cmd, retries + 1);
	}
	return 0;
}

static int send_command_with_retry_extended(const char* cmd, const char* expected1,const char* expected2,const char* not_expected, uint32_t timeout) {
	int retries = 0;
	while (retries < MAX_RETRIES) {

		send_command(cmd);
		if (wait_for_extended(expected1, expected2, not_expected, timeout))
			return 1;
		retries++;
		printf("Retrying command: %s (attempt %d)\n", cmd, retries + 1);
	}
	return 0;
}

void Disconnect_MQTT(void){

	printf("\nAT+CMQTTDISC.........");
	if (!send_command_with_retry("AT+CMQTTDISC=0,120\r\n", "OK", 1000))
		return;

	printf("\nAT+CMQTTREL.........");
	if (!send_command_with_retry("AT+CMQTTREL=0\r\n", "OK", 1000))
		return;

	printf("\nAT+CMQTTSTOP.........");
	if (!send_command_with_retry("AT+CMQTTSTOP\r\n", "OK", 1000))
		return;

}



// Fungsi utama untuk mengirim data parameter cuaca ke SIM7600 melalui MQTT
static int Send_To_SIM7600(void) {

	printf("\nAT+CNMP=38.........");
	send_command_with_retry("AT+CNMP=38\r\n", "OK", 5000);
	// 1. Set APN
	printf("\nAT+CGDCONT=1,\"IP\",\"M2MINTERNET\".........");
	if (!send_command_with_retry("AT+CGDCONT=1,\"IP\",\"M2MINTERNET\"\r\n", "OK", 10000))
		return 0;

	// 2. Konfigurasi SSL
	printf("\nAT+CSSLCFG.........");
	if (!send_command_with_retry("AT+CSSLCFG=\"enableSNI\",0,1\r\n", "OK", 1000))
		return 0;

	// 3. Mulai layanan MQTT
	printf("\nAT+CMQTTSTART.........");
	if (!send_command_with_retry("AT+CMQTTSTART\r\n", "OK", 1000)){
		if(!send_command_with_retry("AT+CMQTTSTART\r\n", "+CMQTTSTART: 23", 1000)){
			Disconnect_MQTT();
			return 0;
		}
	}


	// 4. Akuisisi klien MQTT
	printf("\nAT+CMQTTACCQ.........");
	if (!send_command_with_retry("AT+CMQTTACCQ=0,\"SIMCom_client01\",1\r\n", "OK", 1000)){
		if (!send_command_with_retry("AT+CMQTTACCQ=0,\"SIMCom_client01\",1\r\n", "+CMQTTACCQ: 0,19", 1000)){
			Disconnect_MQTT();
			return 0;
		}
	}

	// 5. Hubungkan ke broker HiveMQ
	char connect_cmd[256];
	sprintf(connect_cmd,
			"AT+CMQTTCONNECT=0,\"tcp://a51ad753198b41b3a4c2f4488d3e409d.s1.eu.hivemq.cloud:8883\",60,1,\"haidaramrurusdan\",\"h18082746R\"\r\n");
	printf("\nAT+CMQTTCONNECT.........");
	if (!send_command_with_retry(connect_cmd, "+CMQTTCONNECT: 0,0", 30000)){
		Disconnect_MQTT();
		return 0;
	}

	// 6. Kirim data parameter cuaca ke topik sensor/cuaca
	char payload[256];
	char cmd[50];
	char topic[] = "sensor/cuaca";

	// Susun payload dengan format: waktu, latitude, longitude, suhu, kelembaban, arah angin,
	// kecepatan angin, tekanan udara, radiasi matahari, curah hujan, water level
	sprintf(payload,
			"waktu:%s,latitude:%.5f,longitude:%.5f,suhu:%.1f,kelembaban:%.1f,arah_angin:%.1f,"
			"kecepatan_angin:%.1f,tekanan_udara:%.1f,radiasi_matahari:%.1f,curah_hujan:%.1f,"
			"water_level:%.1f",
			time_display_buffer, Latitude, Longitude, temp_c, humidity_pct, windDir,
			windSpeed, pressure_mbar, solar_rad, curah_hujan, distance);

	int topic_len = strlen(topic);
	int payload_len = strlen(payload);

	// Kirim topik MQTT
	sprintf(cmd, "AT+CMQTTTOPIC=0,%d", topic_len);
	printf("\nAT+CMQTTTOPIC.........");
	if (!send_mqtt_data(cmd, topic, topic_len)){
		Disconnect_MQTT();
		return 0;
	}

	// Kirim payload MQTT
	sprintf(cmd, "AT+CMQTTPAYLOAD=0,%d", payload_len);
	printf("\nAT+CMQTTPAYLOAD.........");
	if (!send_mqtt_data(cmd, payload, payload_len)){
		Disconnect_MQTT();
		return 0;
	}


	// Publikasikan data
	printf("\nAT+CMQTTPUB.........");
	if (!send_command_with_retry("AT+CMQTTPUB=0,1,60\r\n", "+CMQTTPUB: 0,0", 2000)){
		Disconnect_MQTT();
		return 0;
	}

	Disconnect_MQTT();

	return 1;


}

void Request_GPS_Data(void){


	if(mode_com!=2){
		OK_Update = 0;
		OK_send = 0;
		NEXTION_SendString("lat0", "GPS: NULL");
		NEXTION_SendString("long0", "GPS: NULL");
		HAL_Delay(1000);

		Send_Recent_Latlong();

		OK_Update = 1;
		OK_send = 1;

		flag_update_gps = 0;

		return;
	}


	OK_Update = 0;
	OK_send = 0;

	NEXTION_SendString("lat0", "Tunggu....");
	NEXTION_SendString("long0", "Tunggu....");

	printf("\nAT+CNMP=38.........");
	send_command_with_retry("AT+CNMP=38\r\n", "OK", 5000);

	printf("\nAT+CGDCONT=1,\"IP\",\"M2MINTERNET\".........");
	send_command_with_retry("AT+CGDCONT=1,\"IP\",\"M2MINTERNET\"\r\n", "OK", 10000);

	printf("\nAT+CSSLCFG.........");
	send_command_with_retry("AT+CSSLCFG=\"enableSNI\",0,1\r\n", "OK", 1000);

	printf("\nAT+CSSLCFG.........");
	send_command_with_retry("AT+CSSLCFG=\"enableSNI\",0,1\r\n", "OK", 1000);

	uint8_t flag_next_gps_acc = 1;

	for(int i = 0; i<2;i++){

		printf("\nAT+CGPS=1,1.........");
		send_command_with_retry("AT+CGPS=1,1\r\n", "OK", 10000);

		printf("\nAT+CGNSSINFO.........");
		if(send_command_with_retry_extended("AT+CGNSSINFO\r\n", "E", "W", ",,,,,,,," , 100000)){

			char *dataStr = strstr((char*)sim_rx_buffer, "+CGNSSINFO:");
			dataStr += strlen("+CGNSSINFO: ");
			HAL_Delay(100);

			// Variabel untuk token dan penanda indeks token
			char *token;
			int tokenIndex = 0;
			char *latStr = NULL;
			char *latDir = NULL;
			char *lonStr = NULL;
			char *lonDir = NULL;

			// Tokenisasi berdasarkan koma
			token = strtok(dataStr, ",");
			while (token != NULL) {
				// Berdasarkan format:
				// token[0] : nomor mode (misal "2")
				// token[1] : "07"
				// token[2] : "01"
				// token[3] : "02"
				// token[4] : Latitude dalam format ddmm.mmmmmm (misal "0739.552730")
				// token[5] : Arah latitude (misal "S")
				// token[6] : Longitude dalam format dddmm.mmmmmm (misal "11015.669813")
				// token[7] : Arah longitude (misal "E")
				if (tokenIndex == 4) {
					latStr = token;
				} else if (tokenIndex == 5) {
					latDir = token;
				} else if (tokenIndex == 6) {
					lonStr = token;
				} else if (tokenIndex == 7) {
					lonDir = token;
				}
				token = strtok(NULL, ",");
				tokenIndex++;
			}

			// Pastikan semua data yang diperlukan tersedia
			if (latStr && latDir && lonStr && lonDir) {
				// Parsing latitude
				// Format: ddmm.mmmmmm
				float latValue = atof(latStr);
				int latDegrees = (int)(latValue / 100);
				float latMinutes = latValue - (latDegrees * 100);
				float latDecimal = latDegrees + (latMinutes / 60.0f);
				// Jika arah adalah 'S', jadikan negatif
				if (latDir[0] == 'S' || latDir[0] == 's') {
					latDecimal = -latDecimal;
				}
				Latitude = latDecimal;

				// Parsing longitude
				// Format: dddmm.mmmmmm
				float lonValue = atof(lonStr);
				int lonDegrees = (int)(lonValue / 100);
				float lonMinutes = lonValue - (lonDegrees * 100);
				float lonDecimal = lonDegrees + (lonMinutes / 60.0f);
				// Jika arah adalah 'W', jadikan negatif
				if (lonDir[0] == 'W' || lonDir[0] == 'w') {
					lonDecimal = -lonDecimal;
				}
				Longitude = lonDecimal;

				Write_SD("LOCATION.txt", 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,1,1,
						Latitude, Longitude,"","");

			}

			flag_next_gps_acc = 0;
			break;
		}
	}

	if(flag_next_gps_acc){
		flag_next_gps_acc = 0;
		Get_Time_Internal_RTC();
		uint8_t new_hour = rtcstm.hour + 4;
		Set_Alarm_B_Intenal_RTC (new_hour, 11, 35, 1);
	}

	printf("\nAT+CGPS=0.........");
	send_command_with_retry("AT+CGPS=0\r\n", "OK", 10000);
	Send_Recent_Latlong();

	OK_Update = 1;
	OK_send = 1;
	flag_update_gps = 0;

}

void Request_Update_Time_From_SIM7600(void)
{
	OK_Update = 0;
	OK_send = 0;

	NEXTION_SendString("waktu0", "Tunggu....");
	NEXTION_SendString("tgl0", "Tunggu....");


	printf("\nAT+CNMP=38.........");
	send_command_with_retry("AT+CNMP=38\r\n", "OK", 5000);

	printf("\nAT+CGDCONT=1,\"IP\",\"M2MINTERNET\".........");
	send_command_with_retry("AT+CGDCONT=1,\"IP\",\"M2MINTERNET\"\r\n", "OK", 10000);

	printf("\nAT+CSSLCFG.........");
	if (!send_command_with_retry("AT+CSSLCFG=\"enableSNI\",0,1\r\n", "OK", 1000)){
		Sleep_SIM7600();
		return;
	}


	printf("\nAT+HTTPINIT.........");
	send_command_with_retry("AT+HTTPINIT\r\n", "OK", 500);

	printf("\nAT+HTTPPARA=jam.bmkg.go.id.........");
	send_command_with_retry("AT+HTTPPARA=\"URL\",\"https://jam.bmkg.go.id/JamServer.php\"\r\n", "OK", 2000);

	printf("\nAT+HTTPPARA=CID.........");
	send_command_with_retry("AT+HTTPPARA=\"CID\",1\r\n", "OK", 2000);

	printf("\nAT+HTTPACTION.........");
	send_command_with_retry("AT+HTTPACTION=0\r\n", "+HTTPACTION: 0,200", 5000);



	HAL_Delay(100);

	printf("\nAT+HTTPREAD.........");

	if(send_command_with_retry("AT+HTTPREAD=0,150\r\n", "OK", 5000)){
		uint8_t tgl, bulan, jam, menit, detik;
		uint16_t tahun;
		wait_for("new Date(", 10000);
		char *pDate = strstr((char*)sim_rx_buffer, "new Date(");
		if (pDate != NULL)
		{
			// Pindahkan pointer ke awal angka-angka (setelah "new Date(")

			pDate += strlen("new Date(");
			HAL_Delay(100);

			tgl   = (pDate[10]  - '0') * 10 + (pDate[11]  - '0');
			bulan = (pDate[5]  - '0') * 10 + (pDate[6]  - '0');
			tahun = (pDate[0]  - '0') * 1000 + (pDate[1] - '0') * 100
					+ (pDate[2]  - '0') * 10   + (pDate[3] - '0');
			jam   = (pDate[13] - '0') * 10 + (pDate[14] - '0');
			menit = (pDate[16] - '0') * 10 + (pDate[17] - '0');
			detik = (pDate[19] - '0') * 10 + (pDate[20] - '0') + 2;

			printf("Tanggal: %02d-%02d-%04d\n", tgl, bulan, tahun);
			printf("Waktu: %02d:%02d:%02d\n", jam, menit, detik);

			Set_Time_DS3231(detik, menit, jam, 1, tgl, bulan, tahun);
			Set_Time_Internal_RTC(detik, menit, jam, 1, tgl, bulan, tahun);


		}
		else
		{
			printf("Substring 'new Date(' tidak ditemukan dalam buffer.\n");
		}

	}


	printf("\nAT+HTTPTERM.........");
	send_command_with_retry("AT+HTTPTERM\r\n", "OK", 5000);

	Restart_UART_DMA(&huart4, sim_rx_buffer, SIM_BUFFER_SIZE);

	Send_Recent_Time(OK_send);

	OK_Update = 1;
	OK_send = 1;
}

static int SIM7600_Ready(void){
	printf("\nAT.........");
	if(!send_command_with_retry("AT\r\n", "OK", 10000)){
		return 0;
		NVIC_SystemReset();
	}

	return 1;

}



void Wake_SIM7600(void){
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_2, GPIO_PIN_RESET);
	HAL_Delay(200);
	printf("\nWake Up - SIM7600");
	printf("\nAT+CSCLK=0.........");
	send_command_with_retry("AT+CSCLK=0\r\n", "OK", 1000);

	printf("\nAT+CFUN=1.........");
	send_command_with_retry("AT+CFUN=1\r\n", "OK", 1000);

}

void Sleep_SIM7600(void){
	printf("\nEnter Sleep - SIM7600");

	printf("\nAT+CSCLK=1.........");
	send_command_with_retry("AT+CSCLK=1\r\n", "OK", 1000);

	printf("\nAT+CFUN=0.........");
	send_command_with_retry("AT+CFUN=0\r\n", "OK", 1000);


	HAL_Delay(200);
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_2, GPIO_PIN_SET);

}



#endif





void Communication_Init(void){
	if(mode_com==1){
		NEXTION_SendString("time0", "ESP32 INIT");
		ESP_INIT();
		if(!already_on){
			SIM7600_Ready();
		}
		Sleep_SIM7600();


	}else if(mode_com==2){
		NEXTION_SendString("time0", "SIM7600 INIT");
		if(already_on){
			Wake_SIM7600();
		}



	}
	HAL_Delay(500);

	NEXTION_SendString("t0", "Pembaruan Terakhir");
	NEXTION_SendString("time0", time_display_buffer);
}




/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  MX_SPI1_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_UART4_Init();
  MX_ADC1_Init();
  MX_SDIO_SD_Init();
  MX_FATFS_Init();
  MX_USART6_UART_Init();
  MX_I2C1_Init();
  MX_RTC_Init();
  /* USER CODE BEGIN 2 */

	HAL_Delay(500);
	Restart_UART_DMA(&huart6, uart6_rx_buffer, UART6_RX_BUFFER_SIZE);
	Restart_UART_DMA(&huart3, modbus_rx_buffer, MODBUS_BUFFER_SIZE);
	Restart_UART_DMA(&huart2, esp_rx_buffer, ESP_BUFFER_SIZE);
	Restart_UART_DMA(&huart4, sim_rx_buffer, SIM_BUFFER_SIZE);
	HAL_Delay(500);

	printf("\n\n----------- New Session -----------\n");


	Get_Time_DS3231();
	Set_Time_Internal_RTC(rtcds32.seconds, rtcds32.minutes, rtcds32.hour,
			rtcds32.dayofweek, rtcds32.dayofmonth, rtcds32.month, rtcds32.year);

	Send_Initial_Val();
	Set_Interval(OK_send);
	Initial_Interval(interval);
	Set_Alarm_B_Intenal_RTC (8, 11, 35, 1);
	Communication_Init();
	already_on = 1;





  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1)
	{

		if (flag_periodic_meas) {
			Send_Update_Val(OK_Update);
			Initial_Interval(interval);
			continue;
		}


		if (flag_tipp)
		{
			Read_RK400(curah_hujan, OK_send);
			flag_tipp = 0;
			continue;
		}

		if (flag_update) {
			flag_update = 0;
			Send_Update_Val(OK_Update);
			continue;
		}


		if (flag_reset) {
			Set_Config_Default();
			continue;
		}

		if (flag_interval) {
			Capture_Choosed_Interval(mode_interval, OK_send);
			continue;
		}

		if (flag_com) {
			Capture_Choosed_Comm(mode_com, OK_send);
			continue;
		}

		if (flag_apply) {
			Set_Apply(flag_apply, mode_interval, mode_com);
			continue;
		}

		if (flag_recentval) {
			Send_Recent_Val();
			continue;
		}

		if(flag_delete){

			Delete_File("MEASURE.txt");
			Delete_File("LASTVAL.txt");
			Delete_File("LOCATION.txt");
			continue;

		}

		if(flag_periodic_request_time_update){
			flag_periodic_request_time_update = 0;
			if (mode_com==1){
				Request_Update_Time_From_ESP32();
			}
			if (mode_com==2){
				Request_Update_Time_From_SIM7600();
			}
			continue;

		}

		if(flag_time_update_esp_helper){
			Time_Update_ESP_Helper();
			continue;
		}


		if(flag_recent_time){
			Send_Recent_Time(OK_send);
		}

		if(flag_recent_latlong){
			Send_Recent_Latlong();
			continue;
		}

		if(flag_update_gps){
			Request_GPS_Data();
			continue;
		}

		if(flag_timeupdate_only){
			flag_timeupdate_only=0;

			if (mode_com==1){
				Request_Update_Time_From_ESP32();
			}
			if (mode_com==2){
				Request_Update_Time_From_SIM7600();
			}

			continue;

		}

		if(flag_set_wifi){
			Parsing_Wifi_Credentials();
			continue;
		}

		if(flag_ssid_info){
			Send_Recent_SSID();
			continue;
		}

		if(flag_wake_print){
			flag_wake_print = 0;
			printf("STM32: Wake\n");
			continue;
		}

		if(flag_sleep){
			printf("STM32: Sleep\n");
			HAL_Delay(100);
			HAL_SuspendTick();
			HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
			HAL_ResumeTick();
			continue;
		}


    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	}
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_10;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};
  RTC_AlarmTypeDef sAlarm = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 12;
  sTime.Minutes = 26;
  sTime.Seconds = 0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
  sDate.Month = RTC_MONTH_FEBRUARY;
  sDate.Date = 10;
  sDate.Year = 25;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable the Alarm A
  */
  sAlarm.AlarmTime.Hours = 0;
  sAlarm.AlarmTime.Minutes = 0;
  sAlarm.AlarmTime.Seconds = 0;
  sAlarm.AlarmTime.SubSeconds = 0;
  sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
  sAlarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY;
  sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
  sAlarm.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_WEEKDAY;
  sAlarm.AlarmDateWeekDay = RTC_WEEKDAY_SUNDAY;
  sAlarm.Alarm = RTC_ALARM_A;
  if (HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable the Alarm B
  */
  sAlarm.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_DATE;
  sAlarm.AlarmDateWeekDay = 1;
  sAlarm.Alarm = RTC_ALARM_B;
  if (HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief SDIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_SDIO_SD_Init(void)
{

  /* USER CODE BEGIN SDIO_Init 0 */

  /* USER CODE END SDIO_Init 0 */

  /* USER CODE BEGIN SDIO_Init 1 */

  /* USER CODE END SDIO_Init 1 */
  hsd.Instance = SDIO;
  hsd.Init.ClockEdge = SDIO_CLOCK_EDGE_RISING;
  hsd.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
  hsd.Init.ClockPowerSave = SDIO_CLOCK_POWER_SAVE_DISABLE;
  hsd.Init.BusWide = SDIO_BUS_WIDE_1B;
  hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
  hsd.Init.ClockDiv = 4;
  /* USER CODE BEGIN SDIO_Init 2 */

  /* USER CODE END SDIO_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief UART4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART4_Init(void)
{

  /* USER CODE BEGIN UART4_Init 0 */

  /* USER CODE END UART4_Init 0 */

  /* USER CODE BEGIN UART4_Init 1 */

  /* USER CODE END UART4_Init 1 */
  huart4.Instance = UART4;
  huart4.Init.BaudRate = 115200;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART4_Init 2 */

  /* USER CODE END UART4_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 9600;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief USART6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART6_UART_Init(void)
{

  /* USER CODE BEGIN USART6_Init 0 */

  /* USER CODE END USART6_Init 0 */

  /* USER CODE BEGIN USART6_Init 1 */

  /* USER CODE END USART6_Init 1 */
  huart6.Instance = USART6;
  huart6.Init.BaudRate = 9600;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart6) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART6_Init 2 */

  /* USER CODE END USART6_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
  /* DMA1_Stream2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream2_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);
  /* DMA1_Stream3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
  /* DMA1_Stream5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
  /* DMA1_Stream6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);
  /* DMA2_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);
  /* DMA2_Stream6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream6_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, MODEM_SLEEP_Pin|MODEM_PWR_KEY_Pin|WQ_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(EXT0_ESP32_GPIO_Port, EXT0_ESP32_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CTRL_485_GPIO_Port, CTRL_485_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : MODEM_SLEEP_Pin MODEM_PWR_KEY_Pin WQ_CS_Pin */
  GPIO_InitStruct.Pin = MODEM_SLEEP_Pin|MODEM_PWR_KEY_Pin|WQ_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : EXT0_ESP32_Pin */
  GPIO_InitStruct.Pin = EXT0_ESP32_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(EXT0_ESP32_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : CTRL_485_Pin */
  GPIO_InitStruct.Pin = CTRL_485_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CTRL_485_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PD15 */
  GPIO_InitStruct.Pin = GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : DIN_1_Pin DIN_2_Pin DIN_3_Pin DIN_4_Pin */
  GPIO_InitStruct.Pin = DIN_1_Pin|DIN_2_Pin|DIN_3_Pin|DIN_4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1)
	{
	}
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
