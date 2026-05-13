/*
  DuinoCoin_Wire.ino
  created 10 05 2021
  Original code by: Luiz H. Cassettari
  Updated by: AjTheSpidey
*/

#include <Wire.h>

#if DUCO_ESP8266
#define SDA 4 // D2 - A4 - GPIO4
#define SCL 5 // D1 - A5 - GPIO5
#endif

#if DUCO_ESP32
#define SDA 21
#define SCL 22
#endif

#define WIRE_BUFFER_MAX 32
#define WIRE_CHUNK (WIRE_BUFFER_MAX - 1)
#define WIRE_SCAN_MAX (max_avr_miners + 1)

void wire_setup()
{
  //pinMode(SDA, INPUT_PULLUP);
  //pinMode(SCL, INPUT_PULLUP);
  wire_start();
  wire_readAll();
}

void wire_start()
{
  static bool started = false;
  if (started) return;
  Wire.begin(SDA, SCL);
  Wire.setClock(i2c_wire_clock);
  started = true;
}

void wire_readAll()
{
  for (byte address = 1; address < WIRE_SCAN_MAX; address++ )
  {
    if (wire_exists(address))
    {
      Serial.print("Address: ");
      Serial.println(address);
      wire_readLine(address);
    }
  }
}

void wire_SendAll(String message)
{
  for (byte address = 1; address < WIRE_SCAN_MAX; address++ )
  {
    if (wire_exists(address))
    {
      Serial.print("Address: ");
      Serial.println(address);
      Wire_sendln(address, message);
    }
  }
}

boolean wire_exists(byte address)
{
  wire_start();
  Wire.beginTransmission(address);
  byte error = Wire.endTransmission();
  return (error == 0);
}

void wire_sendJob(byte address, String lastblockhash, String newblockhash, int difficulty)
{
  String job = lastblockhash + "," + newblockhash + "," + difficulty;
  Wire_sendln(address, job);
}

void Wire_sendln(byte address, String message)
{
  Wire_send(address, message + "\n");
}

void Wire_send(byte address, String message)
{
  wire_start();
  const char* data = message.c_str();
  int length = message.length();
  for (int i = 0; i < length; i += WIRE_CHUNK)
  {
    int chunk = min(WIRE_CHUNK, length - i);
    Wire.beginTransmission(address);
    Wire.write((const uint8_t*)data + i, chunk);
    Wire.endTransmission();
  }
}

String wire_readLine(int address)
{
  char end = '\n';
  String str = "";
  unsigned long start = millis();
  wire_start();
  while (millis() - start < i2c_read_timeout_ms)
  {
    Wire.requestFrom(address, 1);
    if (Wire.available())
    {
      char c = Wire.read();
      //Serial.print(c);
      if (c == end)
      {
        break;
      }
      str += c;
    }
    yield();
  }
  //str += end;
  return str;
}

boolean wire_runEvery(unsigned long interval)
{
  static unsigned long previousMillis = 0;
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;
    return true;
  }
  return false;
}
