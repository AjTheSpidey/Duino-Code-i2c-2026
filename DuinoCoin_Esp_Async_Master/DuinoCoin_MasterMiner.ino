/*
  DuinoCoin_MasterMiner.ino
  Lets the ESP master mine its own Duino-Coin job while it keeps brokering I2C slaves.
*/

#include "DSHA1.h"
#include "Counter.h"

#if ESP8266
#define MASTER_JOB_TYPE "ESP8266H"
#define MASTER_MINER_NAME "ESP8266 I2C Master"
#endif

#if ESP32
#if defined(CONFIG_FREERTOS_UNICORE)
#define MASTER_JOB_TYPE "ESP32S"
#else
#define MASTER_JOB_TYPE "ESP32"
#endif
#define MASTER_MINER_NAME "ESP32 I2C Master"
#endif

#define MASTER_CONNECT_EVERY 10000
#define MASTER_READ_TIMEOUT 100
#define MASTER_TIMEOUT 30000

enum MasterMinerState
{
  MASTER_STATE_CONNECT,
  MASTER_STATE_VERSION_WAIT,
  MASTER_STATE_JOB_REQUEST,
  MASTER_STATE_JOB_WAIT,
  MASTER_STATE_HASHING,
  MASTER_STATE_RESULT_WAIT
};

WiFiClient masterClient;
MasterMinerState masterState = MASTER_STATE_CONNECT;
String masterBuffer = "";
String masterLastBlockHash = "";
String masterExpectedHashString = "";
uint8_t masterExpectedHash[20];
uint8_t masterHash[20];
DSHA1 masterBaseSha1;
Counter<10> masterCounter;
unsigned long masterDifficulty = 0;
unsigned long masterJobStartMicros = 0;
unsigned long masterStateTime = 0;
unsigned long masterLastConnectAttempt = 0;
unsigned long masterShares = 0;
unsigned long masterAcceptedShares = 0;
float masterHashrate = 0.0;
String masterDucoId = "";

void masterMiner_state(MasterMinerState state)
{
  masterState = state;
  masterStateTime = millis();
}

String masterMiner_chipId()
{
  #if ESP8266
    String id = String(ESP.getChipId(), HEX);
    id.toUpperCase();
    return id;
  #endif

  #if ESP32
    uint64_t chipId = ESP.getEfuseMac();
    uint16_t high = (uint16_t)(chipId >> 32);
    char id[17];
    snprintf(id, sizeof(id), "%04X%08X", high, (uint32_t)chipId);
    return String(id);
  #endif
}

uint8_t masterMiner_hexNibble(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return 0;
}

bool masterMiner_hexToBytes(String input, uint8_t* output, byte outputLength)
{
  if (input.length() < outputLength * 2) return false;
  for (byte i = 0; i < outputLength; i++) {
    output[i] = (masterMiner_hexNibble(input.charAt(i * 2)) << 4) |
                masterMiner_hexNibble(input.charAt(i * 2 + 1));
  }
  return true;
}

String masterMiner_getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

String masterMiner_readLine()
{
  String line = "";

  while (masterClient.available()) {
    char c = masterClient.read();
    if (c == '\n') {
      line = masterBuffer;
      masterBuffer = "";
      break;
    }
    masterBuffer += c;
  }

  if (masterBuffer.length() > 0 && millis() - masterStateTime > MASTER_READ_TIMEOUT) {
    line = masterBuffer;
    masterBuffer = "";
  }

  return line;
}

void masterMiner_stop()
{
  masterClient.stop();
  masterBuffer = "";
  masterMiner_state(MASTER_STATE_CONNECT);
}

bool masterMiner_connect()
{
  if (masterClient.connected()) return true;
  if (millis() - masterLastConnectAttempt < MASTER_CONNECT_EVERY) return false;

  masterLastConnectAttempt = millis();
  if (host.length() == 0 || port == 0) {
    UpdatePool();
    return false;
  }

  Serial.println(String("[M]Connecting to Duino-Coin server... ") + host + " " + String(port));
  ws_sendAll(String("[M]Connecting to Duino-Coin server... ") + host + " " + String(port));

  masterClient.setTimeout(100);
  if (!masterClient.connect(host.c_str(), port)) {
    Serial.println("[M]Connection failed.");
    UpdatePool();
    return false;
  }

  masterBuffer = "";
  masterMiner_state(MASTER_STATE_VERSION_WAIT);
  return true;
}

void masterMiner_requestJob()
{
  String request = String("JOB,") + String(ducouser) + "," + MASTER_JOB_TYPE;
  if (strlen(minerKey) > 0) request += "," + String(minerKey);
  request += "\n";

  Serial.println(String("[M]Job Request: ") + String(ducouser) + " " + MASTER_JOB_TYPE);
  masterClient.print(request);
  masterMiner_state(MASTER_STATE_JOB_WAIT);
}

bool masterMiner_parseJob(String job)
{
  if (job.indexOf(',') == -1) return false;

  masterLastBlockHash = masterMiner_getValue(job, ',', 0);
  masterExpectedHashString = masterMiner_getValue(job, ',', 1);
  unsigned long rawDifficulty = masterMiner_getValue(job, ',', 2).toInt();
  masterDifficulty = rawDifficulty * 100UL + 1UL;

  if (masterLastBlockHash.length() == 0 || !masterMiner_hexToBytes(masterExpectedHashString, masterExpectedHash, 20)) {
    return false;
  }

  masterBaseSha1.reset().write((const unsigned char*)masterLastBlockHash.c_str(), masterLastBlockHash.length());
  masterCounter.reset();
  masterJobStartMicros = micros();
  return true;
}

void masterMiner_submit(unsigned long result, unsigned long elapsedMicros)
{
  float elapsedSeconds = elapsedMicros * .000001f;
  masterHashrate = elapsedSeconds > 0 ? result / elapsedSeconds : 0;
  masterShares++;

  String identifier = String(rigIdentifier) + " [M]";
  String payload = String(result) + "," +
                   String(masterHashrate, 2) + "," +
                   MASTER_MINER_NAME + "," +
                   identifier + "," +
                   masterDucoId + "\n";

  masterClient.print(payload);
  Serial.print("[M]");
  Serial.print(payload);
  ws_sendAll(String("[M]") + payload);
  masterMiner_state(MASTER_STATE_RESULT_WAIT);
}

unsigned long masterMiner_hashBudget()
{
  if (!miningMode_hasI2C()) return master_hash_us_single;
  if (clients_slaveCount() == 0) return master_hash_us_single;
  return master_hash_us_shared;
}

void masterMiner_hashBatch()
{
  unsigned long sliceStart = micros();
  unsigned long hashBudget = masterMiner_hashBudget();

  while (masterCounter < masterDifficulty && micros() - sliceStart < hashBudget) {
    DSHA1 ctx = masterBaseSha1;
    ctx.write((const unsigned char*)masterCounter.c_str(), masterCounter.strlen()).finalize(masterHash);

    if (memcmp(masterExpectedHash, masterHash, 20) == 0) {
      masterMiner_submit(masterCounter, micros() - masterJobStartMicros);
      blink(BLINK_SHARE_FOUND);
      return;
    }

    ++masterCounter;
  }

  if (!(masterCounter < masterDifficulty)) {
    Serial.println("[M]No result found in job range. Requesting a fresh job.");
    masterMiner_state(MASTER_STATE_JOB_REQUEST);
  }

  handleSystemEvents();
}

void masterMiner_setup()
{
  masterDucoId = String("DUCOID") + masterMiner_chipId();
  masterBaseSha1.warmup();
  masterLastConnectAttempt = millis() - MASTER_CONNECT_EVERY;
  masterMiner_state(MASTER_STATE_CONNECT);
}

void masterMiner_loop()
{
  if (WiFi.status() != WL_CONNECTED) {
    masterMiner_stop();
    return;
  }

  if (!masterClient.connected() && masterState != MASTER_STATE_CONNECT) {
    masterMiner_stop();
  }

  switch (masterState) {
    case MASTER_STATE_CONNECT:
      masterMiner_connect();
      break;

    case MASTER_STATE_VERSION_WAIT: {
      String version = masterMiner_readLine();
      if (version.length() > 0) {
        Serial.println(String("[M]Node version: ") + version);
        masterMiner_state(MASTER_STATE_JOB_REQUEST);
      }
      break;
    }

    case MASTER_STATE_JOB_REQUEST:
      masterMiner_requestJob();
      break;

    case MASTER_STATE_JOB_WAIT: {
      String job = masterMiner_readLine();
      if (job.length() > 0) {
        Serial.println(String("[M]Job ") + job);
        if (masterMiner_parseJob(job)) {
          masterMiner_state(MASTER_STATE_HASHING);
        } else {
          Serial.println("[M]Bad job response. Reconnecting.");
          masterMiner_stop();
        }
      }
      break;
    }

    case MASTER_STATE_HASHING:
      masterMiner_hashBatch();
      break;

    case MASTER_STATE_RESULT_WAIT: {
      String result = masterMiner_readLine();
      if (result.length() > 0) {
        if (result.indexOf("GOOD") != -1) masterAcceptedShares++;
        Serial.println(String("[M]Job ") + result + ": Share #" + String(masterShares));
        ws_sendAll(String("[M]Job ") + result + ": Share #" + String(masterShares));
        masterMiner_state(MASTER_STATE_JOB_REQUEST);
      }
      break;
    }
  }

  if (masterState != MASTER_STATE_HASHING && millis() - masterStateTime > MASTER_TIMEOUT) {
    Serial.println("[M]Timeout. Reconnecting.");
    masterMiner_stop();
  }
}

String masterMiner_status()
{
  String str = "M ";
  str += masterClient.connected() ? String(masterHashrate / 1000.0, 1) + "kH/s" : "offline";
  str += " ";
  str += masterAcceptedShares;
  str += "/";
  str += masterShares;
  return str;
}
