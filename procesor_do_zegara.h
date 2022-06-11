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

// piny 0,1 -> RS232 wyjście dla DEBUG
#define OE_PIN		2	// OE rejestru 74HC595 -> potrzebny?
#define data4_PIN	3	// wejście danych czwartego rejestru
#define HW_PIN		4	// czujnik podczerwieni
#define data3_PIN	5
#define data2_PIN	6
#define data1_PIN	7
#define C18B20_PIN	8	// czujnik temperatury
#define BELL_PIN	9
#define data5_PIN	14	// A0
#define CLK_PIN		15	// A1 clock rejestru
#define STK_PIN		16	// A2 latch pin rejestru
#define data6_PIN	17	// A3

// piny na MCP23008:
#define ClockB_PIN	0	// diody pomiędzy lampami
#define ClockR_PIN	1	// diody pomiędzy lampami
// pin 2 wolny
// pin 3 wolny
#define WEEKR_PIN	4	// dioda dnia tygodnia
#define WEEKG_PIN	5	// dioda dnia tygodnia
#define WEEKB_PIN	6	// dioda dnia tygodnia
#define ClockG_PIN	7	// diody pomiędzy lampami

void wyswietl(byte data1, byte data2, byte data3, byte data4, byte data5, byte data6);
void showZegar();
void pokazDate();
void showTemperatura();
void pokazNic();
void zegar_razem();
void ClockLed(byte kolor);

#endif /* PROCESOR_DO_ZEGARA_H_ */
