

#include <avr/pgmspace.h>

#include <Arduino.h>
#include <ADNS2610.h>

#define LED 13

static const int leds[] = {
  2, 3, 11, 12, -1,
};

#define SDIO 5
#define SCLK 4

static MouseCam mouse(SDIO, SCLK);

void error()
{
  bool state = true;
  while (true) {
    // turn the illumination LEDS on  
    for (int i = 0; ; i++) {
      const int pin = leds[i];
      if (pin == -1)
        break;
      digitalWrite(pin, state ? HIGH : LOW);
    }
    delay(200);
    state = !state;
  }
}

// Include the auto-generated sector file.
// run
//    ./mouse_cam.py -c > sectors.c
// to generate.

extern const int* segs[];

static int find_seg(const byte* frame)
{
    float lowest = 256 * MouseCam::FRAMELENGTH;
    int lowest_seg = -1;

    for (int seg = 0; ; seg++) {
        const int* PROGMEM segment = segs[seg];
        if (!segment)
          break;
        int total = 0;
        int count = 0;
        while (true) {
            const int idx = pgm_read_word(segment);
            if (idx == -1)
              break;
            const byte pixel = frame[idx];
            segment += 1;
            total += pixel;
            count += 1;
        }
        const float average = total / float(count);
        if (average < lowest) {
            lowest = average;
            lowest_seg = seg;
        }
    }
    return lowest_seg;
}

  /*
  *
  */

void setup()
{
  pinMode(LED, OUTPUT);

  // turn the illumination LEDS on  
  for (int i = 0; ; i++) {
    const int pin = leds[i];
    if (pin == -1)
      break;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    delay(200);
  }

  Serial.begin(9600);

  digitalWrite(LED, HIGH);
  if (!mouse.init())
  {
    error();
  }
  digitalWrite(LED, LOW);

  Serial.print("ADNS2610 id=");
  Serial.print(mouse.getId());
  Serial.print("\r\n");
}

void loop()
{
  static bool video = false;
  static bool bad_frame = false;

  if (Serial.available()) {
    int c = Serial.read();
    switch (c)
    {
      case 'v'  :  video = true;   break;
      case 'd'  :  video = false;  break;
    }
  }

  digitalWrite(LED, bad_frame ? HIGH : LOW);
  if (bad_frame)
    bad_frame = false;

  byte frame[MouseCam::FRAMELENGTH];
  const bool okay = mouse.readFrame(frame);

  if (!okay) {
    bad_frame = true;
    if (!video) {
      Serial.print("-1\r\n");
    }
    return; // error reading frame
  }

  if (video) {
    for (int i = 0; i < mouse.FRAMELENGTH; i++)
    {
      Serial.print((char) (frame[i] & 0x3F));
    }
    Serial.print(char(0x80)); // end of frame marker
  }
  else
  {
    const int seg = find_seg(frame);
    Serial.print(seg);
    Serial.print("\r\n");
  }
}

// FIN
