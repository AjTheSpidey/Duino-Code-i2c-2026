#include <Wire.h>
#include <DuinoCoin.h>

#define I2C_ADDRESS 0
#define ADDRESS_FIRST 1
#define ADDRESS_LAST 119

String receiveBuffer;
String responseBuffer = "\n";
byte i2cAddress = 1;

String boardId();

byte chooseAddress() {
  if (I2C_ADDRESS > 0) return I2C_ADDRESS;

  Wire.begin();
  for (byte address = ADDRESS_FIRST; address <= ADDRESS_LAST; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() != 0) {
      Wire.end();
      return address;
    }
  }
  Wire.end();
  return ADDRESS_FIRST;
}

void receiveEvent(int howMany) {
  while (Wire.available()) {
    receiveBuffer += (char)Wire.read();
  }
}

void requestEvent() {
  char c = '\n';
  if (responseBuffer.length() > 0 && responseBuffer.indexOf('\n') != -1) {
    c = responseBuffer.charAt(0);
    responseBuffer.remove(0, 1);
  }
  Wire.write(c);
}

String boardId() {
  #if defined(ARDUINO_GENERIC_F103C8TX) || defined(ARDUINO_BLUEPILL_F103C8)
    return "STM32F103";
  #elif defined(ARDUINO_ARCH_STM32)
    return "STM32";
  #else
    return "MCU";
  #endif
}

void setup() {
  Serial.begin(115200);
  i2cAddress = chooseAddress();
  Wire.begin(i2cAddress);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
  Serial.println(String("STM32 I2C slave address: ") + i2cAddress);
}

void loop() {
  int newline = receiveBuffer.indexOf('\n');
  if (newline == -1) return;

  String job = receiveBuffer.substring(0, newline);
  receiveBuffer.remove(0, newline + 1);

  int first = job.indexOf(',');
  int second = job.indexOf(',', first + 1);
  if (first == -1 || second == -1) return;

  String lastBlockHash = job.substring(0, first);
  String expectedHash = job.substring(first + 1, second);
  unsigned int difficulty = job.substring(second + 1).toInt();

  unsigned long start = micros();
  unsigned int result = Ducos1a.work(lastBlockHash, expectedHash, difficulty);
  unsigned long elapsed = micros() - start;

  responseBuffer = String(result) + "," +
                   String(elapsed) + "," +
                   "DUCOID-" + boardId() + "-" + String(i2cAddress) + "\n";
  Serial.println(responseBuffer);
}
