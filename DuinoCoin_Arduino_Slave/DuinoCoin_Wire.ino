/*
  DoinoCoin_Wire.ino
  created 10 05 2021
  Original code by: Luiz H. Cassettari
  Updated by: AjTheSpidey
*/

#include <Wire.h>
#include <DuinoCoin.h>        // https://github.com/ricaun/arduino-DuinoCoin
#include <ArduinoUniqueID.h>  // https://github.com/ricaun/ArduinoUniqueID

byte i2c = 1;
String bufferReceive = "";
String bufferRequest = "";

String get_BOARD_TYPE();

void DuinoCoin_setup()
{
  UniqueID8dump(Serial);
  
  pinMode(A5, INPUT_PULLUP);
  pinMode(A4, INPUT_PULLUP);
  
  unsigned long time = (unsigned long) getTrueRotateRandomByte() * 1000 + (unsigned long) getTrueRotateRandomByte();
  delayMicroseconds(time);
  Wire.begin();
  for (int address = 1; address < 127; address++ )
  {
    Wire.beginTransmission(address);
    int error = Wire.endTransmission();
    if (error != 0)
    {
      i2c = address;
      break;
    }
  }
  Wire.end();

  // Wire begin
  Wire.begin(i2c);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);

  Serial.print(F("Wire Address: "));
  Serial.println(i2c);
}


void receiveEvent(int howMany) {
  if (howMany == 0)
  {
    return;
  }
  while (Wire.available()) {
    char c = Wire.read();
    bufferReceive += c;
  }
}

void requestEvent() {
  char c = '\n';
  if (bufferRequest.length() > 0 && bufferRequest.indexOf('\n') != -1)
  {
    c = bufferRequest[0];
    bufferRequest.remove(0, 1);
  }
  Wire.write(c);
}

bool DuinoCoin_loop()
{
  int endLine = bufferReceive.indexOf('\n');
  if (bufferReceive.length() > 0 && endLine != -1) {

    Serial.print(F("Job: "));
    Serial.print(bufferReceive);

    String jobLine = bufferReceive.substring(0, endLine);
    bufferReceive.remove(0, endLine + 1);

    int firstComma = jobLine.indexOf(',');
    int secondComma = jobLine.indexOf(',', firstComma + 1);
    if (firstComma == -1 || secondComma == -1) {
      return false;
    }

    String lastblockhash = jobLine.substring(0, firstComma);
    String newblockhash = jobLine.substring(firstComma + 1, secondComma);
    unsigned int difficulty = jobLine.substring(secondComma + 1).toInt();
    // Start time measurement
    unsigned long startTime = micros();
    // Call DUCO-S1A hasher
    unsigned int ducos1result = 0;
    if (difficulty < 655) ducos1result = Ducos1a.work(lastblockhash, newblockhash, difficulty);
    // End time measurement
    unsigned long endTime = micros();
    // Calculate elapsed time
    unsigned long elapsedTime = endTime - startTime;
    // Send result back to the program with share time
    bufferRequest = String(ducos1result) + "," + String(elapsedTime) + "," + String(get_DUCOID()) + "\n";
    
    Serial.print(F("Done "));
    Serial.print(String(ducos1result) + "," + String(elapsedTime) + "," + String(get_DUCOID()) + "\n");

    return true;
  }
  return false;
}

String DuinoCoin_response()
{
  return bufferRequest;
}

// Grab Arduino chip DUCOID
String get_DUCOID() {
  String ID = "DUCOID";
  char buff[4];
  for (size_t i = 0; i < 8; i++)
  {
    sprintf(buff, "%02X", (uint8_t) UniqueID8[i]);
    ID += buff;
  }
  ID += "-";
  ID += get_BOARD_TYPE();
  return ID;
}

String get_BOARD_TYPE() {
  #if defined(ARDUINO_AVR_UNO)
    return "UNO";
  #elif defined(ARDUINO_AVR_NANO)
    return "NANO";
  #elif defined(ARDUINO_AVR_NANO_EVERY)
    return "NANO_EVERY";
  #elif defined(ARDUINO_AVR_MEGA2560)
    return "MEGA2560";
  #elif defined(ARDUINO_AVR_LEONARDO)
    return "LEONARDO";
  #elif defined(ARDUINO_AVR_MICRO)
    return "MICRO";
  #else
    return "AVR";
  #endif
}
