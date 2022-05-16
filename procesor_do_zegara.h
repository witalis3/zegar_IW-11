/*
 * procesor_do_zegara.h
 *
 *  Created on: 26 kwi 2022
 *      Author: witek
 */

#ifndef PROCESOR_DO_ZEGARA_H_
#define PROCESOR_DO_ZEGARA_H_

#define DEBUGo

#define COLDSTART_REF	0x12   		// when started, the firmware examines this "Serial Number"

#define OE_PIN		2	// OE rejestru 74HC595 -> potrzebny?
#define data4_PIN	3	// wejście danych czwartego rejestru
// #define ClockR_PIN	4 -> na MCP23008
#define data3_PIN	5
#define data2_PIN	6
#define data1_PIN	7
#define C18B20_PIN	8	// czujnik temperatury
#define HW_PIN		4	// czujnik podczerwieni
#define data5_PIN	14	// A0
#define CLK_PIN		15	// A1 clock rejestru
#define STK_PIN		16	// A2 latch pin rejestru
#define data6_PIN	17	// A3
#define ClockB_PIN	0
#define ADC2_PIN	1
#define BELL_PIN	9
#define WEEKR_PIN	4
#define WEEKG_PIN	5
#define WEEKB_PIN	6

void wyswietl(byte data1, byte data2, byte data3, byte data4, byte data5, byte data6);
void showZegar();
void pokazDate();
void showTemperatura();
byte decToBcd(byte val);



#endif /* PROCESOR_DO_ZEGARA_H_ */
