
#define LED 12

void setup()
{
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
}

void loop()
{
  static bool on = true;
  
  digitalWrite(LED, on ? LOW : HIGH);
  on = !on;
  delay(500);
}