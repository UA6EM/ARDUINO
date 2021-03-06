
/*
 Copyright (C) 2015 Dave Berkeley projects2@rotwang.co.uk

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 USA
*/

#include <JeeLib.h>

#include <radiodev.h>
#include <radioutils.h>

#include <OneWire.h>
#include <DallasTemperature.h>

  /*
   *
   */

// needed by the watchdog code
EMPTY_INTERRUPT(WDT_vect);

  /*
  *
  */
  
// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 4 // jeenode port 1 digital pin
#define PULLUP_PIN A0  // jeenode port 1 analog pin

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

  /*
  *
  */  

// node -> gateway data
#define PRESENT_TEMPERATURE (1 << 1)
#define PRESENT_VOLTAGE (1 << 2)
#define PRESENT_VCC (1 << 3)

#define VOLTAGE_PIN 1

static const float v_scale = (1.1 * 1000) / 1024;

static int get_voltage(int pin) {
  const uint16_t analog = analogRead(pin);
  //Serial.print(analog);
  //Serial.print("\n");
  const float v = v_scale * analog;
  return int(v);
}

  /*
  *
  */

class VoltageMonitorRadio : public RadioDev
{
public:

  VoltageMonitorRadio()
  : RadioDev(GATEWAY_ID, 7)
  {
  }

  virtual void init()
  {
    RadioDev::init();

    // use the 1.1V internal ref for the ADC
    analogReference(INTERNAL);
    
    pinMode(PULLUP_PIN, INPUT_PULLUP);
    sensors.begin();
  }

  virtual const char* banner()
  {
    return "Voltage Monitor v1.0";
  }

  virtual void append_message(Message* msg)
  {
    sensors.requestTemperatures(); // Send the command to get temperatures
    const float ft = sensors.getTempCByIndex(0);
    const uint16_t t = int(ft * 100);
    msg->append(PRESENT_TEMPERATURE, & t, sizeof(t));

    const uint16_t v = get_voltage(VOLTAGE_PIN);
    msg->append(PRESENT_VOLTAGE, & v, sizeof(v));

    const uint16_t vcc = read_vcc();
    msg->append(PRESENT_VCC, & vcc, sizeof(vcc));
  }

  virtual void loop(void)
  {
    radio_loop(32767);
  }
};

static VoltageMonitorRadio radio;

 /*
  *
  */
  
void setup() 
{
  Serial.begin(57600);
  Serial.println(radio.banner());

  radio.init();
}

 /*
  *
  */

void loop()
{
  radio.loop();
}

// FIN
