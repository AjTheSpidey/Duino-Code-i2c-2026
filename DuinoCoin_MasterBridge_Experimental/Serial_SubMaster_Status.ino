/*
  Experimental sub-master status sketch.
  Use this beside the ESP I2C master when you want a root controller to poll status over Serial.
*/

const bool MASTER_ONLY_MODE = false; // Match the ESP master's master_only setting for this sub-master
const char* SUBMASTER_NAME = "ESP-SUBMASTER";

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
    Serial.println(String("OK,") + SUBMASTER_NAME + "," + (MASTER_ONLY_MODE ? "MASTER-ONLY" : "I2C-MASTER"));
  } else if (command == "PAUSE") {
    Serial.println("OK,PAUSED-STUB");
  } else if (command == "RESUME") {
    Serial.println("OK,RESUMED-STUB");
  } else {
    Serial.println("ERR,UNKNOWN-COMMAND");
  }
}

