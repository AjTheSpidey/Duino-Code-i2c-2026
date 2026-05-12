/*
  DoinoCoin_Esp_Master.ino
  created 10 05 2021
  Original code by: Luiz H. Cassettari
  Updated by: AjTheSpidey
*/

void wire_setup();
void wire_readAll();
boolean wire_exists(byte address);
void wire_sendJob(byte address, String lastblockhash, String newblockhash, int difficulty);
void Wire_sendln(byte address, String message);
void Wire_send(byte address, String message);
String wire_readLine(int address);
boolean wire_runEvery(unsigned long interval);
void masterMiner_setup();
void masterMiner_loop();
String masterMiner_status();
bool miningMode_hasI2C();
void clients_scanSlaves(bool force);
int clients_slaveCount();

// Board detection must happen before any ESP-specific includes in the sketch tabs.
#if defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266)
  #define DUCO_ESP8266 1
  #define DUCO_ESP32 0
#elif defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
  #define DUCO_ESP8266 0
  #define DUCO_ESP32 1
#else
  #error "Select an ESP8266 or ESP32 board for DuinoCoin_Esp_Async_Master."
#endif

// ---------------------- Easy Settings ---------------------- //
const char* user = "your_username";    // Duino-Coin username
const char* rig = "Duino-I2C-2026";    // Rig name shown in the wallet
const char* mining_key = "";           // Wallet mining key, leave empty if disabled

const char* wifi_1_name = "your_wifi_name";          // Main WiFi SSID
const char* wifi_1_pass = "your_wifi_password";      // Main WiFi password
const char* wifi_2_name = "";          // Backup WiFi SSID
const char* wifi_2_pass = "";          // Backup WiFi password
const char* wifi_3_name = "";          // Backup WiFi SSID
const char* wifi_3_pass = "";          // Backup WiFi password

const bool auto_i2c_slaves = true;      // Master always mines; I2C slaves are detected automatically
const bool master_turbo_when_solo = true; // No slaves online: hash continuously and only pause for WiFi/OTA
const byte max_avr_miners = 16;        // More than 10 is supported; 16 is safer for ESP RAM
const unsigned long master_hash_us_single = 250000; // Full-power solo slice when no I2C slaves are online
const unsigned long master_hash_us_shared = 20000;  // Shared slice when ESP and I2C slaves mine together
const unsigned long i2c_scan_empty_ms = 15000;      // Empty-bus scan delay; bigger = faster solo mining
const unsigned long i2c_scan_active_ms = 5000;      // Active-bus scan delay; smaller = faster slave detection
// ----------------------------------------------------------- //

struct WifiCredential {
  const char* ssid;
  const char* password;
};

const WifiCredential wifiCredentials[] = {
  {wifi_1_name, wifi_1_pass},
  {wifi_2_name, wifi_2_pass},
  {wifi_3_name, wifi_3_pass}
};

const char* ducouser      = user;
const char* minerKey      = mining_key;
const char* rigIdentifier = rig;

#if DUCO_ESP8266
#include <ESP8266WiFi.h> // Include WiFi library
#include <ESP8266mDNS.h> // OTA libraries
#include <WiFiUdp.h>
#endif
#if DUCO_ESP32
#include <WiFi.h>
#endif

#include <ArduinoOTA.h>
#include <StreamString.h>

#define BLINK_SHARE_FOUND    1
#define BLINK_SETUP_COMPLETE 2
#define BLINK_CLIENT_CONNECT 3
#define BLINK_RESET_DEVICE   5

#if DUCO_ESP8266
#define LED_BUILTIN 2
#define MINER "AVR I2C v3.2"
#define JOB "AVR"
#endif

#if DUCO_ESP32
#define LED_BUILTIN 2
#define MINER "AVR I2C v3.2"
#define JOB "AVR"
#endif

#define WIFI_CONNECT_TIMEOUT 15000

void handleSystemEvents(void) {
  ArduinoOTA.handle();
  yield();
}

bool miningMode_hasI2C()
{
  return auto_i2c_slaves;
}

void SetupWifi() {
  WiFi.mode(WIFI_STA); // Setup ESP in client mode

  #if DUCO_ESP8266
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
  #endif
  #if DUCO_ESP32
    WiFi.setSleep(false);
  #endif

  const byte wifiCount = sizeof(wifiCredentials) / sizeof(wifiCredentials[0]);
  bool hasConfiguredWifi = false;
  for (byte i = 0; i < wifiCount; i++) {
    if (strlen(wifiCredentials[i].ssid) > 0) {
      hasConfiguredWifi = true;
      break;
    }
  }

  while (WiFi.status() != WL_CONNECTED) {
    if (!hasConfiguredWifi) {
      Serial.println("Connecting using saved WiFi credentials...");
      WiFi.begin();
      unsigned long startAttempt = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < WIFI_CONNECT_TIMEOUT) {
        delay(500);
        Serial.print(".");
        handleSystemEvents();
      }
      Serial.println();
    } else {
      for (byte i = 0; i < wifiCount && WiFi.status() != WL_CONNECTED; i++) {
        if (strlen(wifiCredentials[i].ssid) == 0) continue;

        Serial.println(String("Connecting to WiFi #") + String(i + 1) + ": " + String(wifiCredentials[i].ssid));
        WiFi.disconnect();
        delay(250);
        WiFi.begin(wifiCredentials[i].ssid, wifiCredentials[i].password);

        unsigned long startAttempt = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < WIFI_CONNECT_TIMEOUT) {
          delay(500);
          Serial.print(".");
          handleSystemEvents();
        }
        Serial.println();
      }
    }

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi connection failed. Retrying configured networks...");
      delay(1000);
    }
  }

  Serial.println("\nConnected to WiFi!");
  Serial.println(String("Local IP address: ") + WiFi.localIP().toString());
}

void SetupOTA() {
  ArduinoOTA.onStart([]() { // Prepare OTA stuff
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

#if DUCO_ESP8266
  char hostname[32];
  sprintf(hostname, "Miner-Async-%06x", ESP.getChipId());
  ArduinoOTA.setHostname(hostname);
#endif

#if DUCO_ESP32
  char hostname[32];
  sprintf(hostname, "Miner32-Async-%06x", (uint32_t)ESP.getEfuseMac());
  ArduinoOTA.setHostname(hostname);
#endif

  ArduinoOTA.begin();
}

void blink(uint8_t count, uint8_t pin = LED_BUILTIN) {
  uint8_t state = HIGH;
  for (int x = 0; x < (count << 1); ++x) {
    digitalWrite(pin, state ^= HIGH);
    delay(50);
  }
}

void RestartESP(String msg) {
  Serial.println(msg);
  Serial.println("Resetting ESP...");
  blink(BLINK_RESET_DEVICE);
  #if DUCO_ESP8266
    ESP.reset();
  #endif
  #if DUCO_ESP32
    ESP.restart();
  #endif
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  Serial.print("\nDuino-Coin ");
  Serial.println(MINER);

  #if DUCO_ESP8266
    system_update_cpu_freq(160);
    os_update_cpu_frequency(160);
  #endif
  #if DUCO_ESP32
    setCpuFrequencyMhz(240);
  #endif

  oled_setup();

  if (miningMode_hasI2C()) {
    wire_setup();
    clients_scanSlaves(true);
  }
  SetupWifi();
  SetupOTA();

  oled_display(WiFi.localIP().toString() + "\n" + String(ESP.getFreeHeap()) + "\n" + clients_string());

  server_setup();

  UpdatePool();
  masterMiner_setup();

  blink(BLINK_SETUP_COMPLETE);
}

void loop() {
  ArduinoOTA.handle();
  if (miningMode_hasI2C()) clients_loop();
  masterMiner_loop();
  if (runEvery(1000))
  {
    String status = clients_string() + " " + masterMiner_status();
    Serial.print("[ ]");
    Serial.println(String("FreeRam: ") + String(ESP.getFreeHeap()) + " " + status);
    ws_sendAll(String("FreeRam: ") + String(ESP.getFreeHeap()) + " - " + status);
    oled_display(WiFi.localIP().toString() + "\n" + String(ESP.getFreeHeap()) + "\n" + status);
  }
}

boolean runEvery(unsigned long interval)
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
