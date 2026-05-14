/*
  PC I2C Bridge MCU
  USB serial bridge between a PC pool client and I2C mining slaves.
*/

#include <Wire.h>

const unsigned long serial_baud = 115200;
const unsigned long i2c_wire_clock = 100000;
const byte first_i2c_address = 1;
const byte last_i2c_address = 16;
const byte wire_chunk = 31;

String serialLine = "";

String getField(String data, char separator, int index) {
  int found = 0;
  int start = 0;
  for (unsigned int i = 0; i <= data.length(); i++) {
    if (i == data.length() || data[i] == separator) {
      if (found == index) return data.substring(start, i);
      found++;
      start = i + 1;
    }
  }
  return "";
}

bool i2cExists(byte address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void scanI2C() {
  Serial.print("ADDRS");
  for (byte address = first_i2c_address; address <= last_i2c_address; address++) {
    if (i2cExists(address)) {
      Serial.print(",");
      Serial.print(address);
    }
  }
  Serial.println();
}

void sendI2CLine(byte address, String line) {
  line += "\n";
  for (unsigned int i = 0; i < line.length(); i += wire_chunk) {
    Wire.beginTransmission(address);
    for (byte j = 0; j < wire_chunk && i + j < line.length(); j++) {
      Wire.write(line[i + j]);
    }
    byte error = Wire.endTransmission();
    if (error != 0) {
      Serial.print("ERR,I2C_WRITE_");
      Serial.println(error);
      return;
    }
  }
  Serial.println("OK");
}

void readI2CLine(byte address, unsigned long timeoutMs) {
  String line = "";
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    Wire.requestFrom(address, (byte)1);
    if (Wire.available()) {
      char c = Wire.read();
      if (c == '\n') {
        Serial.print("DATA,");
        Serial.println(line);
        return;
      }
      line += c;
    }
    yield();
  }
  Serial.println("TIMEOUT");
}

void handleCommand(String command) {
  command.trim();
  if (command == "SCAN") {
    scanI2C();
    return;
  }

  if (command.startsWith("JOB,")) {
    byte address = getField(command, ',', 1).toInt();
    String lastHash = getField(command, ',', 2);
    String expectedHash = getField(command, ',', 3);
    String difficulty = getField(command, ',', 4);
    if (address == 0 || lastHash.length() == 0 || expectedHash.length() == 0 || difficulty.length() == 0) {
      Serial.println("ERR,BAD_JOB");
      return;
    }
    sendI2CLine(address, lastHash + "," + expectedHash + "," + difficulty);
    return;
  }

  if (command.startsWith("READ,")) {
    byte address = getField(command, ',', 1).toInt();
    unsigned long timeoutMs = getField(command, ',', 2).toInt();
    if (timeoutMs == 0) timeoutMs = 5000;
    readI2CLine(address, timeoutMs);
    return;
  }

  Serial.println("ERR,UNKNOWN_COMMAND");
}

void setup() {
  Serial.begin(serial_baud);
  Wire.begin();
  Wire.setClock(i2c_wire_clock);
  Serial.println("READY,PC-I2C-BRIDGE");
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      handleCommand(serialLine);
      serialLine = "";
    } else if (c != '\r') {
      serialLine += c;
    }
  }
}
