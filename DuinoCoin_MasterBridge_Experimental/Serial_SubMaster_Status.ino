/*
  Experimental sub-master status sketch.
  Use this beside the ESP I2C master when you want a root controller to poll status over Serial.
*/

void setup() {
  Serial.begin(115200);
}

void loop() {
  if (!Serial.available()) return;

  String command = Serial.readStringUntil('\n');
  command.trim();

  if (command == "PING") {
    Serial.println("OK,PONG");
  } else if (command == "STATUS") {
    Serial.println("OK,ESP-SUBMASTER,STATUS-STUB");
  } else if (command == "PAUSE") {
    Serial.println("OK,PAUSED-STUB");
  } else if (command == "RESUME") {
    Serial.println("OK,RESUMED-STUB");
  } else {
    Serial.println("ERR,UNKNOWN-COMMAND");
  }
}

