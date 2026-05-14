/*
  UNO R4 WiFi I2C Master
  Experimental Duino-Coin WiFi + I2C master for WiFiS3 boards.
*/

#include <WiFiS3.h>
#include <Wire.h>
#include <DuinoCoin.h>

const char* user = "your_username";
const char* rig = "UNO-R4-WiFi-I2C-2026";
const char* mining_key = "";

const char* wifi_1_name = "your_wifi_name";
const char* wifi_1_pass = "your_wifi_password";
const char* wifi_2_name = "";
const char* wifi_2_pass = "";
const char* wifi_3_name = "";
const char* wifi_3_pass = "";

const bool master_only = false;
const bool auto_i2c_slaves = true;
const bool master_mines_with_i2c_slaves = true;
const byte max_i2c_slaves = 16;
const unsigned long i2c_scan_empty_ms = 15000;
const unsigned long i2c_scan_active_ms = 30000;
const unsigned long i2c_read_timeout_ms = 5000;
const unsigned long i2c_wire_clock = 100000;

const char* pool_host = "server.duinocoin.com";
const int pool_port = 2811;
const char* local_job_type = "ESP32S";
const char* slave_job_type = "AVR";
const char* local_miner_name = "UNO R4 WiFi I2C Master";
const char* slave_miner_name = "Official AVR Miner 4.3";

WiFiClient masterClient;
byte slaves[16];
byte slaveCount = 0;
unsigned long lastScan = 0;

struct WifiCredential {
  const char* ssid;
  const char* pass;
};

WifiCredential wifi[] = {
  {wifi_1_name, wifi_1_pass},
  {wifi_2_name, wifi_2_pass},
  {wifi_3_name, wifi_3_pass}
};

String readPoolLine(WiFiClient& client) {
  String line = "";
  unsigned long start = millis();
  while (client.connected() && millis() - start < 30000) {
    while (client.available()) {
      char c = client.read();
      if (c == '\n') return line;
      line += c;
    }
    delay(1);
  }
  return line;
}

bool connectClient(WiFiClient& client) {
  if (client.connected()) return true;
  client.stop();
  if (!client.connect(pool_host, pool_port)) return false;
  readPoolLine(client);
  return true;
}

String requestJob(WiFiClient& client, const char* jobType) {
  if (!connectClient(client)) return "";
  String req = String("JOB,") + user + "," + jobType;
  if (strlen(mining_key) > 0) req += "," + String(mining_key);
  client.print(req + "\n");
  return readPoolLine(client);
}

String submitShare(WiFiClient& client, String payload) {
  if (!connectClient(client)) return "OFFLINE";
  client.print(payload + "\n");
  return readPoolLine(client);
}

void connectWifi() {
  for (byte i = 0; i < 3; i++) {
    if (strlen(wifi[i].ssid) == 0) continue;
    Serial.println(String("WiFi: ") + wifi[i].ssid);
    WiFi.begin(wifi[i].ssid, wifi[i].pass);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) delay(250);
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println(WiFi.localIP());
      return;
    }
  }
}

bool i2cExists(byte address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void scanSlaves(bool force = false) {
  unsigned long interval = slaveCount ? i2c_scan_active_ms : i2c_scan_empty_ms;
  if (!force && millis() - lastScan < interval) return;
  lastScan = millis();
  slaveCount = 0;
  for (byte address = 1; address <= max_i2c_slaves; address++) {
    if (i2cExists(address) && slaveCount < sizeof(slaves)) slaves[slaveCount++] = address;
  }
  Serial.println(String("I2C slaves: ") + slaveCount);
}

void sendI2CLine(byte address, String line) {
  line += "\n";
  for (unsigned int i = 0; i < line.length(); i += 31) {
    Wire.beginTransmission(address);
    for (byte j = 0; j < 31 && i + j < line.length(); j++) Wire.write(line[i + j]);
    Wire.endTransmission();
  }
}

String readI2CLine(byte address) {
  String line = "";
  unsigned long start = millis();
  while (millis() - start < i2c_read_timeout_ms) {
    Wire.requestFrom(address, (byte)1);
    if (Wire.available()) {
      char c = Wire.read();
      if (c == '\n') break;
      line += c;
    }
    delay(1);
  }
  return line;
}

void serveSlave(byte address) {
  WiFiClient client;
  String job = requestJob(client, slave_job_type);
  if (job.indexOf(',') == -1) return;
  sendI2CLine(address, job);
  String result = readI2CLine(address);
  int a = result.indexOf(',');
  int b = result.indexOf(',', a + 1);
  if (a < 0 || b < 0) return;
  long nonce = result.substring(0, a).toInt();
  long elapsed = result.substring(a + 1, b).toInt();
  String ducoid = result.substring(b + 1);
  float rate = nonce / (elapsed * .000001f);
  String payload = String(nonce) + "," + String(rate, 2) + "," + slave_miner_name + "," +
                   String(rig) + " [" + String(address) + "]," + ducoid;
  Serial.println(String("[") + address + "] " + submitShare(client, payload));
}

void mineLocal() {
  if (slaveCount && !master_mines_with_i2c_slaves) return;
  String job = requestJob(masterClient, local_job_type);
  if (job.indexOf(',') == -1) return;
  String lastHash = job.substring(0, job.indexOf(','));
  String rest = job.substring(job.indexOf(',') + 1);
  String expected = rest.substring(0, rest.indexOf(','));
  unsigned int difficulty = rest.substring(rest.indexOf(',') + 1).toInt() * 100 + 1;
  unsigned long start = micros();
  unsigned int nonce = Ducos1a.work(lastHash, expected, difficulty);
  unsigned long elapsed = micros() - start;
  if (nonce > 0) {
    float rate = nonce / (elapsed * .000001f);
    String payload = String(nonce) + "," + String(rate, 2) + "," + local_miner_name + "," +
                     String(rig) + ",DUCOID-UNOR4WIFI";
    Serial.println(String("[M] ") + submitShare(masterClient, payload));
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(i2c_wire_clock);
  connectWifi();
  scanSlaves(true);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWifi();
  if (!master_only && auto_i2c_slaves) {
    scanSlaves();
    for (byte i = 0; i < slaveCount; i++) serveSlave(slaves[i]);
  }
  mineLocal();
}
