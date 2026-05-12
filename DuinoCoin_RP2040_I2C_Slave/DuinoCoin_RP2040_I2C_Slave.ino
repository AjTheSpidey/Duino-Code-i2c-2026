#include <Wire.h>
#include <DuinoCoin.h>

#define I2C_ADDRESS 0x12

String receiveBuffer;
String responseBuffer = "\n";

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

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_ADDRESS);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
  Serial.println(String("RP2040 I2C slave address: ") + I2C_ADDRESS);
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

  responseBuffer = String(result) + "," + String(elapsed) + ",DUCOID-RP2040-" + String(I2C_ADDRESS) + "\n";
  Serial.println(responseBuffer);
}

