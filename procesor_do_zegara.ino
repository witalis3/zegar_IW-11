/*
 * atrapa procesora do zegarka na lampkach fluoroscencyjnych IW-11
 */
#include "Arduino.h"
#include "procesor_do_zegara.h"

#include <TimerOne.h>
#include "LPD6803.h"

#include <EEPROM.h>

#include "Adafruit_MCP23008.h"
Adafruit_MCP23008 mcp;

#include <DS3231.h>
#include <Wire.h>
DS3231 zegar;		// układ zegarka
bool century = false;
bool h12Flag;
bool pmFlag;
byte year;
byte month;
byte date;
byte dOW;
byte hour;
byte minute;
byte second;
bool weekDayLedOn;
//bool byla_zmiana = false;
//unsigned long czas_zmiany;


#include <OneWire.h>
#include "DallasTemperature.h"
// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS C18B20_PIN
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

/*
 * Choose the library to be used for IR receiving
 */
//#define USE_TINY_IR_RECEIVER // Recommended, but only for NEC protocol!!! If disabled and IRMP_INPUT_PIN is defined, the IRMP library is used for decoding

#define DECODE_NEC          // Includes Apple and Onkyo
#define IR_INPUT_PIN    HW_PIN
#include <IRremote.hpp>
#define DEBUG_BUTTON_PIN 10		// używany przez SPI

//#include <SPI.h>
#ifdef FASTLED
#include "FastLED.h"
#define NUM_LEDS 1
#define DATA_PIN 11
#define CLOCK_PIN 13
CRGB leds[NUM_LEDS];
#endif

// Choose which 2 pins you will use for output.
// Can be any valid output pins.
int dataPin = 11;       // 'yellow' wire
int clockPin = 13;      // 'green' wire
// Don't forget to connect 'blue' to ground and 'red' to +5V

// Timer 1 is also used by the strip to send pixel clocks

// Set the first variable to the NUMBER of pixels. 20 = 20 pixels in a row
LPD6803 strip = LPD6803(1, dataPin, clockPin);
enum
{
	OFF,
	ODDECH,
	RED,
	GREEN,
	BLUE,
	TRYB_NUM
};
byte trybLedRGB = ODDECH;

unsigned int i, j, k;


byte segmenty[10] = {B01101111, B01000001, B01110110, B01110011, B01011001, B00111011, B00111111, B01100001, B01111111, B01111011};
byte segmenty_z_p[10] = {B11101111, B11000001, B11110110, B11110011, B11011001, B10111011, B10111111, B11100001, B11111111, B11111011};

// kody rozkazów z pilota:
const unsigned int AutoDisplayCommand = 0x4A;
// zmienne pamiętane w EEPROM:
bool AutoDisplay = true;
byte czasCzasu = 30;		// czas trwania wyświetlania czasu (godziny, minuty, sekund)
byte czasDaty = 5;			// czas wyświetlania daty
byte czasTemperatury = 5; 	// czas wyświetlania temperatury
bool bylaPokazana = false;	// czy już była pokazana temperatura w danym oknie czasowym
unsigned long teraz;
unsigned long poczatek;


void setup()
{
#ifdef DEBUGo
	Serial.begin(115200);
	Serial.println("start zegara...");
#endif
	if (EEPROM.read(30) != COLDSTART_REF)
	{
		EEPROM.write(30, COLDSTART_REF); // COLDSTART_REF in first byte indicates all initialized
		EEPROM.write(31, AutoDisplay);
		EEPROM.write(32, czasCzasu);
		EEPROM.write(33, czasDaty);
		EEPROM.write(34, czasTemperatury);
		EEPROM.write(35, weekDayLedOn);
		EEPROM.write(36, trybLedRGB);
#ifdef DEBUGo
		Serial.println("writing initial values into memory");
#endif
	}
	else                       // EEPROM contains stored data, retrieve the data
	{
		// odczyt trybu AutoDisplay
		AutoDisplay = EEPROM.read(31);
		czasCzasu = EEPROM.read(32);
		czasDaty = EEPROM.read(33);
		czasTemperatury = EEPROM.read(34);
		weekDayLedOn = EEPROM.read(35);
		trybLedRGB = EEPROM.read(36);
#ifdef DEBUGo
		Serial.println("reading AutoDisplay from memory: ");
		Serial.println(AutoDisplay);
		Serial.print("czasCzasu: ");
		Serial.println(czasCzasu);
		Serial.print("czasDaty: ");
		Serial.println(czasDaty);
		Serial.print("czasTemperatury: ");
		Serial.println(czasTemperatury);
#endif
	}

	Wire.begin();
	mcp.begin(0);
	mcp.pinMode(WEEKR_PIN, OUTPUT);
	mcp.pinMode(WEEKG_PIN, OUTPUT);
	mcp.pinMode(WEEKB_PIN, OUTPUT);

	pinMode(HW_PIN, INPUT_PULLUP);
	pinMode(OE_PIN, OUTPUT);
	digitalWrite(OE_PIN, LOW);		// shift register output enable
	pinMode(data1_PIN, OUTPUT);		// cyfra jednostek sekund
	pinMode(data2_PIN, OUTPUT);
	pinMode(data3_PIN, OUTPUT);
	pinMode(data4_PIN, OUTPUT);
	pinMode(data5_PIN, OUTPUT);
	pinMode(data6_PIN, OUTPUT);		// cyfra dziesiątek godzin
	pinMode(CLK_PIN, OUTPUT);
	pinMode(STK_PIN, OUTPUT);

	IrReceiver.begin(HW_PIN, false);

	// Start up the 18B20 library czujnik temperatury
	sensors.begin();

	// ledy:
#ifdef FASTLED
    FastLED.addLeds<LPD6803, DATA_PIN, CLOCK_PIN, GRB>(leds, NUM_LEDS);  // GRB ordering is typical
#endif
    // The Arduino needs to clock out the data to the pixels
    // this happens in interrupt timer 1, we can change how often
    // to call the interrupt. setting CPUmax to 100 will take nearly all all the
    // time to do the pixel updates and a nicer/faster display,
    // especially with strands of over 100 dots.
    // (Note that the max is 'pessimistic', its probably 10% or 20% less in reality)

    strip.setCPUmax(50);  // start with 50% CPU usage. up this if the strand flickers or is slow

    // Start up the LED counter
    strip.begin();

    // Update the strip, to start they are all 'off'
    strip.show();

	poczatek = millis();
}

void loop()
{
	switch (trybLedRGB)
	{
		case OFF:
			strip.setPixelColor(0, 0, 0, 0);
			strip.show();
			zegar_razem();
			break;
		case ODDECH:
			  for (j=0; j < 96 * 3; j++)
			  {     // 3 cycles of all 96 colors in the wheel
			    for (i=0; i < strip.numPixels(); i++)
			    {
			      strip.setPixelColor(i, Wheel( (i + j) % 96));
			    }
			    strip.show();   // write all the pixels out
				zegar_razem();
				delay(50);
			  }
			break;
		case RED:
			strip.setPixelColor(0, 31, 0, 0);
			strip.show();
			zegar_razem();
			break;
		case GREEN:
			strip.setPixelColor(0, 0, 31, 0);
			strip.show();
			zegar_razem();
			break;
		case BLUE:
			strip.setPixelColor(0, 0, 0, 31);
			strip.show();
			zegar_razem();
			break;
		default:
			zegar_razem();
			break;
	}
	zegar_razem();
	delay(50);
}
void zegar_razem()
{
	teraz = millis() - poczatek;	// czas względem początku
	unsigned long czasCzasuL = czasCzasu*1000;
	unsigned long czasDatyL = czasDaty*1000;
	unsigned long czasTemperaturyL = czasTemperatury*1000;
	if (teraz > 0 and teraz <= czasCzasuL)
	{
		showZegar();
	}
	else if (teraz > czasCzasuL and teraz <= czasCzasuL + czasDatyL)
	{
		pokazDate();
	}
	else if (teraz > czasCzasuL + czasDatyL and teraz <= czasCzasuL + czasDatyL + czasTemperaturyL)
	{
		if (not bylaPokazana)
		{
			showTemperatura();
			bylaPokazana = true;
		}
	}
	else	// nowy cykl
	{
		bylaPokazana = false;
		poczatek = millis();
		pokazNic();
	}
	// dioda dnia tygodnia WeekDay: dzień 1 -> niedziela świeci led czerwony, ..., dzień 7 -> sobota świecą wszysstkie trzy ledy
	byte weekDay = zegar.getDoW();
#ifdef DEBUG
	Serial.print("weekDay: ");
	Serial.println(weekDay);
#endif
	if (weekDayLedOn)
	{
		mcp.digitalWrite(WEEKR_PIN, !((weekDay & B00000001) == B00000001));
		mcp.digitalWrite(WEEKG_PIN, !((weekDay & B00000010) == B00000010));
		mcp.digitalWrite(WEEKB_PIN, !((weekDay & B00000100) == B00000100));
	}
	else
	{
		mcp.digitalWrite(WEEKR_PIN, HIGH);
		mcp.digitalWrite(WEEKG_PIN, HIGH);
		mcp.digitalWrite(WEEKB_PIN, HIGH);
	}

    /*
     * Check if received data is available and if yes, try to decode it.
     * Decoded result is in the IrReceiver.decodedIRData structure.
     *
     * E.g. command is in IrReceiver.decodedIRData.command
     * address is in command is in IrReceiver.decodedIRData.address
     * and up to 32 bit raw data in IrReceiver.decodedIRData.decodedRawData
     */
    if (IrReceiver.decode())
    {
#ifdef DEBUGi
        // Print a short summary of received data
        IrReceiver.printIRResultShort(&Serial);
        if (IrReceiver.decodedIRData.protocol == UNKNOWN)
        {
            // We have an unknown protocol here, print more info
            IrReceiver.printIRResultRawFormatted(&Serial, true);
        }
        Serial.println();
#endif
        /*
         * !!!Important!!! Enable receiving of the next value,
         * since receiving has stopped after the end of the current received data packet.
         */
        //IrReceiver.resume(); // Enable receiving of the next value

        /*
         * Finally, check the received data and perform actions according to the received command
         */
		if (IrReceiver.decodedIRData.address == 0)
		{
			unsigned int rozkaz = IrReceiver.decodedIRData.command;
			if (rozkaz != 0x00)
			{
#ifdef DEBUGo
				Serial.print("rozkaz: ");
				Serial.println(rozkaz, HEX);
#endif
				switch (rozkaz)
				{
				case AutoDisplayCommand:
					if (AutoDisplay)
						AutoDisplay = false;
					else
						AutoDisplay = true;
					//tone(BELL_PIN, 750, 200);
					break;
				case 0x19:	// minus
					k = k - 1;
					Serial.print("k = ");
					Serial.println(k);
					break;
				case 0x40:	// plus
					k = k + 1;
					Serial.print("k = ");
					Serial.println(k);
					break;
				case 0x16:	// "0" włączanie/wyłączanie week LED
					weekDayLedOn = !weekDayLedOn;
					EEPROM.write(35, weekDayLedOn);
#ifdef DEBUGo
					Serial.println("zapisano weekDayLedOn");
#endif
					break;
				case 0x08:	// "4" przełączanie trybów diod LED pod lampami IV-11
					trybLedRGB = trybLedRGB + 1;
					j = 96*3;
					if (trybLedRGB == TRYB_NUM)
					{
						trybLedRGB = OFF;
					}
					EEPROM.write(36, trybLedRGB);
#ifdef DEBUGo
					Serial.print("zapisano trybLedRGB: ");
					Serial.println(trybLedRGB);
#endif
					break;
				default:
					break;
				}
				delay(200);
			}
		}
        IrReceiver.resume(); // Enable receiving of the next value
	} // koniec if (IrReceiver.decode())

}
/*
 * wyświetlanie 6 znaków na 6 lampach IW-12
 */
void wyswietl(byte data1 = 0x0, byte data2 = 0x0, byte data3 = 0x0, byte data4 = 0x0, byte data5 = 0x0, byte data6 = 0x0)
{
	byte i;
	for (i = 0; i < 8; i++)
	{
		digitalWrite(data1_PIN, (data1 & 128) != 0);
		data1 <<= 1;
		digitalWrite(data2_PIN, (data2 & 128) != 0);
		data2 <<= 1;
		digitalWrite(data3_PIN, (data3 & 128) != 0);
		data3 <<= 1;
		digitalWrite(data4_PIN, (data4 & 128) != 0);
		data4 <<= 1;
		digitalWrite(data5_PIN, (data5 & 128) != 0);
		data5 <<= 1;
		digitalWrite(data6_PIN, (data6 & 128) != 0);
		data6 <<= 1;

		digitalWrite(CLK_PIN, HIGH);
		digitalWrite(CLK_PIN, LOW);
	}
    digitalWrite(STK_PIN, HIGH);
    digitalWrite(STK_PIN, LOW);
}
/*
 * wyświetlanie aktualnej daty
 */
void pokazDate()
{
	date = zegar.getDate();
	byte dziesiatkiDni = ~segmenty[date/10];
	byte jednostkiDni = ~segmenty[date%10];
	month = zegar.getMonth(century);
	byte dziesiatkiMiesiecy = ~segmenty[month/10];
	byte jednostkiMiesiecy = ~segmenty[month%10];
	year = zegar.getYear();
	byte dziesiatkiLat = ~segmenty[year/10];
	byte jednostkiLat = ~segmenty[year%10];
	wyswietl(dziesiatkiDni, jednostkiDni, dziesiatkiMiesiecy, jednostkiMiesiecy, dziesiatkiLat, jednostkiLat);
}
/*
 * wyświetlanie aktualnego czasu
 */
void showZegar()
{
	hour = zegar.getHour(h12Flag, pmFlag);
	byte dziesiatkiGodzin = ~segmenty[hour/10];		// pierwsza cyfra
	byte jednostkiGodzin = ~segmenty[hour%10];		// druga cyfra
	minute = zegar.getMinute();
	byte dziesiatkiMinut = ~segmenty[minute/10];	// trzecia cyfra
	byte jednostkiMinut = ~segmenty[minute%10];		// czwarta cyfra
	second = zegar.getSecond();
	byte dziesiatkiSekund = ~segmenty[second/10];	// piąta cyfra
	byte jednostkiSekund = ~segmenty[second%10];	// szósta cyfra
	wyswietl(dziesiatkiGodzin, jednostkiGodzin, dziesiatkiMinut, jednostkiMinut, dziesiatkiSekund, jednostkiSekund);
}

/*
 * pomiar i wyświetlenie temperatury
 * wyświetlenie zależne od zmiennej AutoDisplay
 *
 */
void showTemperatura()
{
	// pomiar temperatury
	// call sensors.requestTemperatures() to issue a global temperature
	// request to all devices on the bus
	//Serial.print("Requesting temperatures...");
	sensors.requestTemperatures(); // Send the command to get temperatures
	//Serial.println("DONE");
	// After we got the temperatures, we can print them here.
	// We use the function ByIndex, and as an example get the temperature from the first sensor only.
	float tempC = sensors.getTempCByIndex(0);
	// Check if reading was successful
	if (tempC != DEVICE_DISCONNECTED_C)
	{
#ifdef DEBUGo
		Serial.println(tempC);
#endif
		int tempI = 10 * tempC;
		byte dziesiatkiTemperatury = ~segmenty[tempI/100];
		tempI = tempI % 100;
		byte jednostkiTemperatury = ~segmenty_z_p[tempI/10];
		byte dziesiateTemperatury = ~segmenty[tempI%10];
		wyswietl(0xFF,0xFF ,dziesiatkiTemperatury , jednostkiTemperatury, dziesiateTemperatury, 0xFF);
	}
	else
	{
		// wyświetla "EE"
		wyswietl(0xFF, 0xFF, B11000001, B11000001, 0xFF, 0xFF);
#ifdef DEBUGo
		Serial.println("Error: Could not read temperature data");
#endif
	}
}
void pokazNic()
{
	wyswietl(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
}

// Slightly different, this one makes the rainbow wheel equally distributed
// along the chain

// fill the dots one after the other with said color
// good for testing purposes

/* Helper functions */

// Create a 15 bit color value from R,G,B
unsigned int Color(byte r, byte g, byte b)
{
  //Take the lowest 5 bits of each value and append them end to end
  return ( (((unsigned int)g & 0x1F )<<10) | (((unsigned int)b & 0x1F)<<5) | ((unsigned int)r & 0x1F));
}

//Input a value 0 to 127 to get a color value.
//The colours are a transition r - g -b - back to r
unsigned int Wheel(byte WheelPos)
{
  byte r,g,b;
  switch(WheelPos >> 5)
  {
    case 0:
      r=31- WheelPos % 32;   //Red down
      g=WheelPos % 32;      // Green up
      b=0;                  //blue off
      break;
    case 1:
      g=31- WheelPos % 32;  //green down
      b=WheelPos % 32;      //blue up
      r=0;                  //red off
      break;
    case 2:
      b=31- WheelPos % 32;  //blue down
      r=WheelPos % 32;      //red up
      g=0;                  //green off
      break;
  }
  return(Color(r,g,b));
}
