#include "Arduino.h"
#include "procesor_do_zegara.h"

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
bool wyswietlGodzine = false;
bool wyswietlDate = false;
bool wyswietlTemperature = false;

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
#define DEBUG_BUTTON_PIN 10

#include <SPI.h>
#include "LPD6803_SPI.h"
LPD6803_SPI strip = LPD6803_SPI(6);		// sterowanie diodami RGB

byte segmenty[10] = {B01101111, B01000001, B01110110, B01110011, B01011001, B00111011, B00111111, B01100001, B01111111, B01111011};
byte segmenty_z_p[10] = {B11101111, B11000001, B11110110, B11110011, B11011001, B10111011, B10111111, B11100001, B11111111, B11111011};

// kody rozkazów z pilota:
const unsigned int AutoDisplayCommand = 0x4A;
// zmienne pamiętane w EEPROM:
bool AutoDisplay = true;
byte czasCzasu = 30;		// czas trwania wyświetlania czasu (godziny, minuty, sekund)
byte czasDaty = 5;			// czas wyświetlania daty
byte czasTemperatury = 5; 	// czas wyświetlania temperatury



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
	  strip.begin(SPI_CLOCK_DIV16); // 8 Mhz
	  strip.show();
}

void loop()
{
	strip.setPixelColor(1, Color(0,0,0));
	strip.show();
	if (wyswietlGodzine)
	{
		showZegar();
	}
	if (wyswietlDate)
	{
		pokazDate();
	}
	if (wyswietlTemperature)
	{
		showTemperatura();
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
        IrReceiver.resume(); // Enable receiving of the next value

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
				case 0x15:

					break;
				default:
					break;
				}
			}
		}
	} // if (IrReceiver.decode())

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
	/*
	uint8_t i;
	for (i = 0; i < 8; i++)
	{
		digitalWrite(data1_PIN, (dziesiatkiDni & 128) != 0);
		dziesiatkiDni <<= 1;
		digitalWrite(data2_PIN, (jednostkiDni & 128) != 0);
		jednostkiDni <<= 1;
		digitalWrite(data3_PIN, (dziesiatkiMiesiecy & 128) != 0);
		dziesiatkiMiesiecy <<= 1;
		digitalWrite(data4_PIN, (jednostkiMiesiecy & 128) != 0);
		jednostkiMiesiecy <<= 1;
		digitalWrite(data5_PIN, (dziesiatkiLat & 128) != 0);
		dziesiatkiLat <<= 1;
		digitalWrite(data6_PIN, (jednostkiLat & 128) != 0);
		jednostkiLat <<= 1;

		digitalWrite(CLK_PIN, HIGH);
		digitalWrite(CLK_PIN, LOW);
	}
    digitalWrite(STK_PIN, HIGH);
    digitalWrite(STK_PIN, LOW);
    */
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
	/*
	uint8_t i;
	for (i = 0; i < 8; i++)
	{
		digitalWrite(data1_PIN, (dziesiatkiGodzin & 128) != 0);
		dziesiatkiGodzin <<= 1;
		digitalWrite(data2_PIN, (jednostkiGodzin & 128) != 0);
		jednostkiGodzin <<= 1;
		digitalWrite(data3_PIN, (dziesiatkiMinut & 128) != 0);
		dziesiatkiMinut <<= 1;
		digitalWrite(data4_PIN, (jednostkiMinut & 128) != 0);
		jednostkiMinut <<= 1;
		digitalWrite(data5_PIN, (dziesiatkiSekund & 128) != 0);
		dziesiatkiSekund <<= 1;
		digitalWrite(data6_PIN, (jednostkiSekund & 128) != 0);
		jednostkiSekund <<= 1;

		digitalWrite(CLK_PIN, HIGH);
		digitalWrite(CLK_PIN, LOW);
	}
    digitalWrite(STK_PIN, HIGH);
    digitalWrite(STK_PIN, LOW);
    */
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
		Serial.println(tempC);
		int tempI = 10 * tempC;
		byte dziesiatkiTemperatury = ~segmenty[tempI/100];
		tempI = tempI % 100;
		byte jednostkiTemperatury = ~segmenty_z_p[tempI/10];
		byte dziesiateTemperatury = ~segmenty[tempI%10];
		wyswietl(0xFF,0xFF ,dziesiatkiTemperatury , jednostkiTemperatury, dziesiateTemperatury, 0xFF);
		/*
		uint8_t i;
		for (i = 0; i < 8; i++)
		{
			digitalWrite(data1_PIN, HIGH);
			digitalWrite(data2_PIN, HIGH);
			digitalWrite(data3_PIN, (dziesiatkiTemperatury & 128) != 0);
			dziesiatkiTemperatury <<= 1;
			digitalWrite(data4_PIN, (jednostkiTemperatury & 128) != 0);
			jednostkiTemperatury <<= 1;
			digitalWrite(data5_PIN, (dziesiateTemperatury & 128) != 0);
			dziesiateTemperatury <<= 1;
			digitalWrite(data6_PIN, HIGH);

			digitalWrite(CLK_PIN, HIGH);
			digitalWrite(CLK_PIN, LOW);
		}
		digitalWrite(STK_PIN, HIGH);
		digitalWrite(STK_PIN, LOW);
		*/
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
byte decToBcd(byte val)
{
// Convert normal decimal numbers to binary coded decimal
	return ( (val/10*16) + (val%10) );
}

void rainbow(uint8_t wait) {
  int i, j;

  for (j=0; j < 96 * 3; j++) {     // 3 cycles of all 96 colors in the wheel
    for (i=0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel( (i + j) % 96));
    }
    strip.show();   // write all the pixels out
    delay(wait);
  }
}

// Slightly different, this one makes the rainbow wheel equally distributed
// along the chain
void rainbowCycle(uint8_t wait) {
  int i, j;

  for (j=0; j < 96 * 5; j++) {     // 5 cycles of all 96 colors in the wheel
    for (i=0; i < strip.numPixels(); i++) {
      // tricky math! we use each pixel as a fraction of the full 96-color wheel
      // (thats the i / strip.numPixels() part)
      // Then add in j which makes the colors go around per pixel
      // the % 96 is to make the wheel cycle around
      strip.setPixelColor(i, Wheel( ((i * 96 / strip.numPixels()) + j) % 96) );
    }
    strip.show();   // write all the pixels out
    delay(wait);
  }
}

// fill the dots one after the other with said color
// good for testing purposes
void colorWipe(uint16_t c, uint8_t wait) {
  int i;

  for (i=0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, c);
      strip.show();
      delay(wait);
  }
}

/* Helper functions */

// Create a 15 bit color value from R,G,B
unsigned int Color(byte r, byte g, byte b)
{
  //Take the lowest 5 bits of each value and append them end to end
  return( ((unsigned int)g & 0x1F )<<10 | ((unsigned int)b & 0x1F)<<5 | (unsigned int)r & 0x1F);
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
