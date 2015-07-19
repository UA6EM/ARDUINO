
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

#include <radionet.h>

  /*
   *
   */

static const char* banner = "Test Device v1.0";

// node -> gateway data
#define PRESENT_TEMPERATURE (1 << 1)

#define TEMPERATURE_PIN 0

Port led(2);

static uint16_t ack_id;
static byte my_node = 0;

#define ACK_WAIT_MS 100
#define ACK_RETRIES 5

static uint8_t retries;

static Message message(0, GATEWAY_ID);
static uint32_t wait_until = 0;

// needed by the watchdog code
EMPTY_INTERRUPT(WDT_vect);

 /*
  * IO
  */

static void ok_led(byte on) 
{
  led.digiWrite(on);
}

static void test_led(byte on) 
{
  led.digiWrite2(!on);
}

 /*
  *  Temperature 
  */

static const float temp_scale = (1.1 * 100) / 1024;

static int get_temperature(int pin) {
  static int t;
  return ++t * 100;
  //uint16_t analog = analogRead(pin);
  //const float t = temp_scale * analog;
  //return int(t * 100);
}

  /*
  *
  */
  
class Radio {
public:
  typedef enum {
    START=0,
    SLEEP,
    SENDING,
    WAIT_FOR_ACK,
  } STATE;

  STATE state;

  Radio()
  {
  }
  
  void init(void)
  {
    my_node = rf12_configSilent();
  }

  static const int SLEEP_TIME = 32000;

  void sleep(uint16_t time=SLEEP_TIME)
  {
    ok_led(0);
    test_led(0);
    rf12_sleep(0); // turn the radio off
    state = SLEEP;
    //Serial.flush(); // wait for output to complete
    Sleepy::loseSomeTime(time);
  }
};

static Radio radio;

 /*
  * Fall into a deep sleep
  */

static const int LONG_WAIT = 1;

 /*
  * Build a data Message 
  */

void make_message(Message* msg, int msg_id, bool ack) 
{
  msg->reset();
  msg->set_dest(GATEWAY_ID);
  msg->set_mid(msg_id);
  if (ack)
    msg->set_ack();

  const uint16_t t = get_temperature(TEMPERATURE_PIN);
  msg->append(PRESENT_TEMPERATURE, & t, sizeof(t));
}

 /*
  *
  */
  
void setup() 
{
  ok_led(0);
  test_led(0);
  
  led.mode(OUTPUT);  
  led.mode2(OUTPUT);  

  Serial.begin(57600);
  Serial.println(banner);

  //my_node = rf12_configSilent();
  radio.init();

  // use the 1.1V internal ref for the ADC
  analogReference(INTERNAL);

  radio.state = Radio::START;
}

 /*
  *
  */

void loop()
{
  static uint16_t changes = 0;
  
  ok_led(1); // show we are awake

  if (rf12_recvDone() && (rf12_crc == 0)) {
    Message m((void*) & rf12_data[0]);

    if (m.get_dest() == my_node) {
      if (m.get_ack()) {
        // ack the info
        ack_id = m.get_mid();
      }
      else
      {
        ack_id = 0;
        if (radio.state == Radio::WAIT_FOR_ACK) {
          // if we have our ack, go back to sleep
          if (m.get_admin()) {
            radio.state = Radio::START;
            test_led(1);
          } else {
            if (m.get_mid() == message.get_mid()) {
              radio.sleep();
            }
          }
        }
      }
    }
  }

  if (radio.state == Radio::START) {
    Serial.print("hello\r\n");
    send_text(banner, ack_id, false);
    rf12_sendWait(0);
    ack_id = 0;
    test_led(0);
    radio.sleep();
    return;
  }

  if (radio.state == Radio::SLEEP) {
      Serial.println("send");
      radio.state = Radio::SENDING;
      retries = ACK_RETRIES;
      make_message(& message, make_mid(), true);      
      // turn the radio on
      rf12_sleep(-1);
      return;
  }

  if (radio.state == Radio::SENDING) {
    if (rf12_canSend()) {
      // report the change
      send_message(& message);
      rf12_sendWait(0); // NORMAL when finished
      radio.state = Radio::WAIT_FOR_ACK;
      wait_until = millis() + ACK_WAIT_MS;
    }
    return;
  }

  if (radio.state == Radio::WAIT_FOR_ACK) {
    if (millis() > wait_until)
    {
        if (--retries)
            radio.state = Radio::SENDING; // try again
        else
            radio.sleep();
    }
  }
}
 
// FIN
