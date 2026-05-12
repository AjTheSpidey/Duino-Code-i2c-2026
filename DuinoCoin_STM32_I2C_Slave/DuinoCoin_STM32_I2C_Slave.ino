#include <Wire.h>
#include <DuinoCoin.h>

#define I2C_ADDRESS 0

String receiveBuffer;
String responseBuffer = "\n";
byte i2cAddress = 1;

byte chooseAddress() {
  if (I2C_ADDRESS > 0) return I2C_ADDRESS;
  for (byte address = 1; address < 120; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() != 0) return address;
  }
  return 1;
}

void receiveEvent(int howMany) {
  while (Wire.available()) {
    receiveBuffer += (char)Wire.read();
  }
}

void requestEvent() {
  char c = '\n';
  int newline = responseBuffer.indexOf('\n');
  if (responseBuffer.length() > 0 && newline != -1) {
    c = responseBuffer.charAt(0);
    responseBuffer.remove(0, 1);
  }
  Wire.write(c);
}

String boardId() {
  #if defined(ARDUINO_ARCH_STM32)
    return "STM32";
  #else
    return "MCU";
  #endif
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  i2cAddress = chooseAddress();
  Wire.end();
  Wire.begin(i2cAddress);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
  Serial.println(String("STM32 I2C address: ") + i2cAddress);
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

  responseBuffer = String(result) + "," + String(elapsed) + ",DUCOID-" + boardId() + "-" + String(i2cAddress) + "\n";
  Serial.println(responseBuffer);
}

