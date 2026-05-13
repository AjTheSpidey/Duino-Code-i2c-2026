/*
  DuinoCoin_MasterMiner.ino
  Lets the ESP master mine its own Duino-Coin job while it keeps brokering I2C slaves.
*/

#ifndef DUINOCOIN_MASTERMINER_H
#define DUINOCOIN_MASTERMINER_H

#pragma GCC optimize("-Ofast")

#include "DSHA1.h"
#include "Counter.h"

struct MasterMiningLane;

#if DUCO_ESP8266
#define MASTER_JOB_TYPE "ESP8266H"
#define MASTER_MINER_NAME "ESP8266 I2C Master"
#endif

#if DUCO_ESP32
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
#define MASTER_COUNTER_STEP2(counter) do { ++counter; ++counter; } while (false)

const byte MASTER_STATE_CONNECT = 0;
const byte MASTER_STATE_VERSION_WAIT = 1;
const byte MASTER_STATE_JOB_REQUEST = 2;
const byte MASTER_STATE_JOB_WAIT = 3;
const byte MASTER_STATE_HASHING = 4;
const byte MASTER_STATE_RESULT_WAIT = 5;

WiFiClient masterClient;
byte masterState = MASTER_STATE_CONNECT;
#if DUCO_ESP32
TaskHandle_t masterMinerTaskHandle = NULL;
bool masterMinerSecondCore = false;

struct MasterMiningLane {
  WiFiClient client;
  byte state;
  String buffer;
  String lastBlockHash;
  String expectedHashString;
  uint8_t expectedHash[20];
  uint8_t hash[20];
  DSHA1 baseSha1;
  Counter<10> counter;
  unsigned long difficulty;
  unsigned long jobStartMicros;
  unsigned long stateTime;
  unsigned long lastConnectAttempt;
  unsigned long shares;
  unsigned long acceptedShares;
  float hashrate;
  byte lane;
};

MasterMiningLane masterLane2;
#endif
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
unsigned long masterMainHashes = 0;
unsigned long masterShares = 0;
unsigned long masterAcceptedShares = 0;
float masterHashrate = 0.0;
String masterDucoId = "";

void masterMiner_state(byte state)
{
  masterState = state;
  masterStateTime = millis();
}

#if DUCO_ESP32
void masterLane_state(MasterMiningLane &lane, byte state)
{
  lane.state = state;
  lane.stateTime = millis();
}
#endif

String masterMiner_chipId()
{
  #if DUCO_ESP8266
    String id = String(ESP.getChipId(), HEX);
    id.toUpperCase();
    return id;
  #endif

  #if DUCO_ESP32
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

#if DUCO_ESP32
void masterLane_stop(MasterMiningLane &lane)
{
  lane.client.stop();
  lane.buffer = "";
  masterLane_state(lane, MASTER_STATE_CONNECT);
}
#endif

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
  masterMainHashes = 0;
  masterJobStartMicros = micros();
  return true;
}

void masterMiner_submit(unsigned long result, unsigned long elapsedMicros, unsigned long submitHashes, unsigned long displayHashes)
{
  float elapsedSeconds = elapsedMicros * .000001f;
  unsigned long submitWork = submitHashes > 0 ? submitHashes : result;
  unsigned long displayWork = displayHashes > 0 ? displayHashes : submitWork;
  float submitHashrate = elapsedSeconds > 0 ? submitWork / elapsedSeconds : 0;
  masterHashrate = elapsedSeconds > 0 ? displayWork / elapsedSeconds : 0;
  masterShares++;

  String identifier = String(rigIdentifier);
  String payload = String(result) + "," +
                   String(submitHashrate, 2) + "," +
                   MASTER_MINER_NAME + "," +
                   identifier + "," +
                   masterDucoId + "\n";

  masterClient.print(payload);
  Serial.print("[M]");
  Serial.print(payload);
  ws_sendAll(String("[M]") + payload);
  masterMiner_state(MASTER_STATE_RESULT_WAIT);
}

void masterMiner_submit(unsigned long result, unsigned long elapsedMicros)
{
  masterMiner_submit(result, elapsedMicros, result, result);
}

void masterMiner_submit(unsigned long result, unsigned long elapsedMicros, unsigned long hashesDone)
{
  masterMiner_submit(result, elapsedMicros, hashesDone, hashesDone);
}

unsigned long masterMiner_hashBudget()
{
  if (master_only) return master_hash_us_master_only;
  if (!miningMode_hasI2C()) return master_hash_us_single;
  if (clients_slaveCount() == 0) return master_hash_us_single;
  return master_hash_us_shared;
}

bool masterMiner_soloMode()
{
  if (master_only) return true;
  return !miningMode_hasI2C() || clients_slaveCount() == 0;
}

bool masterMiner_enabledNow()
{
  if (master_only) return true;
  if (!master_mines_with_i2c_slaves && miningMode_hasI2C() && clients_slaveCount() > 0) return false;
  return true;
}

void masterMiner_hashBatch()
{
  unsigned long sliceStart = micros();
  unsigned long hashBudget = masterMiner_hashBudget();
  bool soloMode = masterMiner_soloMode();

  while (masterCounter < masterDifficulty) {
    DSHA1 ctx = masterBaseSha1;
    ctx.write((const unsigned char*)masterCounter.c_str(), masterCounter.strlen()).finalize(masterHash);

    if (memcmp(masterExpectedHash, masterHash, 20) == 0) {
      masterMiner_submit(masterCounter, micros() - masterJobStartMicros);
      blink(BLINK_SHARE_FOUND);
      return;
    }

    ++masterCounter;

    if (micros() - sliceStart >= hashBudget) {
      handleSystemEvents();
      if (!soloMode || !master_turbo_when_solo) break;
      sliceStart = micros();
    }
  }

  if (!(masterCounter < masterDifficulty)) {
    Serial.println("[M]No result found in job range. Requesting a fresh job.");
    masterMiner_state(MASTER_STATE_JOB_REQUEST);
  }

  if (!soloMode || !master_turbo_when_solo) handleSystemEvents();
}

void masterMiner_setup()
{
  masterDucoId = String("DUCOID") + masterMiner_chipId();
  masterBaseSha1.warmup();
  masterLastConnectAttempt = millis() - MASTER_CONNECT_EVERY;
  masterMiner_state(MASTER_STATE_CONNECT);
}

bool masterMiner_usesDedicatedTask()
{
  return false;
}

#if DUCO_ESP32
String masterLane_readLine(MasterMiningLane &lane)
{
  String line = "";

  while (lane.client.available()) {
    char c = lane.client.read();
    if (c == '\n') {
      line = lane.buffer;
      lane.buffer = "";
      break;
    }
    lane.buffer += c;
  }

  if (lane.buffer.length() > 0 && millis() - lane.stateTime > MASTER_READ_TIMEOUT) {
    line = lane.buffer;
    lane.buffer = "";
  }

  return line;
}

bool masterLane_connect(MasterMiningLane &lane)
{
  if (lane.client.connected()) return true;
  if (millis() - lane.lastConnectAttempt < MASTER_CONNECT_EVERY) return false;

  lane.lastConnectAttempt = millis();
  if (host.length() == 0 || port == 0) return false;

  Serial.println(String("[M") + String(lane.lane) + "]Connecting to Duino-Coin server... " + host + " " + String(port));
  lane.client.setTimeout(100);
  if (!lane.client.connect(host.c_str(), port)) {
    Serial.println(String("[M") + String(lane.lane) + "]Connection failed.");
    return false;
  }

  lane.buffer = "";
  masterLane_state(lane, MASTER_STATE_VERSION_WAIT);
  return true;
}

void masterLane_requestJob(MasterMiningLane &lane)
{
  String request = String("JOB,") + String(ducouser) + "," + MASTER_JOB_TYPE;
  if (strlen(minerKey) > 0) request += "," + String(minerKey);
  request += "\n";

  Serial.println(String("[M") + String(lane.lane) + "]Job Request: " + String(ducouser) + " " + MASTER_JOB_TYPE);
  lane.client.print(request);
  masterLane_state(lane, MASTER_STATE_JOB_WAIT);
}

bool masterLane_parseJob(MasterMiningLane &lane, String job)
{
  if (job.indexOf(',') == -1) return false;

  lane.lastBlockHash = masterMiner_getValue(job, ',', 0);
  lane.expectedHashString = masterMiner_getValue(job, ',', 1);
  unsigned long rawDifficulty = masterMiner_getValue(job, ',', 2).toInt();
  lane.difficulty = rawDifficulty * 100UL + 1UL;

  if (lane.lastBlockHash.length() == 0 || !masterMiner_hexToBytes(lane.expectedHashString, lane.expectedHash, 20)) {
    return false;
  }

  lane.baseSha1.reset().write((const unsigned char*)lane.lastBlockHash.c_str(), lane.lastBlockHash.length());
  lane.counter.reset();
  lane.jobStartMicros = micros();
  return true;
}

void masterLane_submit(MasterMiningLane &lane, unsigned long result, unsigned long elapsedMicros)
{
  float elapsedSeconds = elapsedMicros * .000001f;
  lane.hashrate = elapsedSeconds > 0 ? result / elapsedSeconds : 0;
  lane.shares++;

  String identifier = String(rigIdentifier);
  String payload = String(result) + "," +
                   String(lane.hashrate, 2) + "," +
                   MASTER_MINER_NAME + "," +
                   identifier + "," +
                   masterDucoId + "\n";

  lane.client.print(payload);
  Serial.print(String("[M") + String(lane.lane) + "]");
  Serial.print(payload);
  ws_sendAll(String("[M") + String(lane.lane) + "]" + payload);
  masterLane_state(lane, MASTER_STATE_RESULT_WAIT);
}

void masterLane_hashBatch(MasterMiningLane &lane)
{
  unsigned long sliceStart = micros();
  unsigned long hashBudget = masterMiner_hashBudget();
  bool soloMode = masterMiner_soloMode();

  while (lane.counter < lane.difficulty) {
    DSHA1 ctx = lane.baseSha1;
    ctx.write((const unsigned char*)lane.counter.c_str(), lane.counter.strlen()).finalize(lane.hash);

    if (memcmp(lane.expectedHash, lane.hash, 20) == 0) {
      masterLane_submit(lane, lane.counter, micros() - lane.jobStartMicros);
      blink(BLINK_SHARE_FOUND);
      return;
    }

    ++lane.counter;

    if (micros() - sliceStart >= hashBudget) {
      if (!soloMode || !master_turbo_when_solo) break;
      taskYIELD();
      sliceStart = micros();
    }
  }

  if (!(lane.counter < lane.difficulty)) {
    Serial.println(String("[M") + String(lane.lane) + "]No result found in job range. Requesting a fresh job.");
    masterLane_state(lane, MASTER_STATE_JOB_REQUEST);
  }
}

void masterLane_loop(MasterMiningLane &lane)
{
  if (!masterMiner_enabledNow()) {
    if (lane.client.connected() || lane.state != MASTER_STATE_CONNECT) masterLane_stop(lane);
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    masterLane_stop(lane);
    return;
  }

  if (!lane.client.connected() && lane.state != MASTER_STATE_CONNECT) {
    masterLane_stop(lane);
  }

  switch (lane.state) {
    case MASTER_STATE_CONNECT:
      masterLane_connect(lane);
      break;

    case MASTER_STATE_VERSION_WAIT: {
      String version = masterLane_readLine(lane);
      if (version.length() > 0) {
        Serial.println(String("[M") + String(lane.lane) + "]Node version: " + version);
        masterLane_state(lane, MASTER_STATE_JOB_REQUEST);
      }
      break;
    }

    case MASTER_STATE_JOB_REQUEST:
      masterLane_requestJob(lane);
      break;

    case MASTER_STATE_JOB_WAIT: {
      String job = masterLane_readLine(lane);
      if (job.length() > 0) {
        Serial.println(String("[M") + String(lane.lane) + "]Job " + job);
        if (masterLane_parseJob(lane, job)) {
          masterLane_state(lane, MASTER_STATE_HASHING);
        } else {
          Serial.println(String("[M") + String(lane.lane) + "]Bad job response. Reconnecting.");
          masterLane_stop(lane);
        }
      }
      break;
    }

    case MASTER_STATE_HASHING:
      masterLane_hashBatch(lane);
      break;

    case MASTER_STATE_RESULT_WAIT: {
      String result = masterLane_readLine(lane);
      if (result.length() > 0) {
        if (result.indexOf("GOOD") != -1) lane.acceptedShares++;
        Serial.println(String("[M") + String(lane.lane) + "]Job " + result + ": Share #" + String(lane.shares));
        ws_sendAll(String("[M") + String(lane.lane) + "]Job " + result + ": Share #" + String(lane.shares));
        masterLane_state(lane, MASTER_STATE_JOB_REQUEST);
      }
      break;
    }
  }

  if (lane.state != MASTER_STATE_HASHING && millis() - lane.stateTime > MASTER_TIMEOUT) {
    Serial.println(String("[M") + String(lane.lane) + "]Timeout. Reconnecting.");
    masterLane_stop(lane);
  }
}

void masterMiner_task(void* parameter)
{
  (void)parameter;
  for (;;) {
    masterLane_loop(masterLane2);
    vTaskDelay(1);
  }
}
#endif

void masterMiner_startTask()
{
  #if DUCO_ESP32
    if (!master_use_second_core || ESP.getChipCores() < 2) return;
    if (masterMinerTaskHandle != NULL) return;

    masterLane2.lane = 2;
    masterLane2.state = MASTER_STATE_CONNECT;
    masterLane2.lastConnectAttempt = millis() - MASTER_CONNECT_EVERY;
    masterLane2.baseSha1.warmup();

    int loopCore = xPortGetCoreID();
    int miningCore = loopCore == 0 ? 1 : 0;
    BaseType_t created = xTaskCreatePinnedToCore(
      masterMiner_task,
      "duco-hash2",
      6144,
      NULL,
      1,
      &masterMinerTaskHandle,
      miningCore
    );

    if (created == pdPASS) {
      masterMinerSecondCore = true;
      Serial.println(String("[M]Second-core hash worker started on core ") + String(miningCore) +
                     " of " + String(ESP.getChipCores()));
    } else {
      Serial.println("[M]Second-core hash worker failed; using one hash lane.");
    }
  #endif
}

void masterMiner_loop()
{
  if (!masterMiner_enabledNow()) {
    if (masterClient.connected() || masterState != MASTER_STATE_CONNECT) {
      masterMiner_stop();
    }
    return;
  }

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
  if (!masterMiner_enabledNow()) {
    str += "paused";
    return str;
  }
  float totalHashrate = masterHashrate;
  unsigned long totalAccepted = masterAcceptedShares;
  unsigned long totalShares = masterShares;
  #if DUCO_ESP32
    totalHashrate += masterLane2.hashrate;
    totalAccepted += masterLane2.acceptedShares;
    totalShares += masterLane2.shares;
  #endif
  str += (masterClient.connected()
          #if DUCO_ESP32
            || masterLane2.client.connected()
          #endif
         ) ? String(totalHashrate / 1000.0, 1) + "kH/s" : "offline";
  str += " ";
  str += totalAccepted;
  str += "/";
  str += totalShares;
  return str;
}

#endif
