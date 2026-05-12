/*
  DuinoCoin_Clients.ino
  created 10 05 2021
  by Luiz H. Cassettari
*/

#if ESP8266
#include <ESP8266WiFi.h> // Include WiFi library
#include <ESP8266mDNS.h> // OTA libraries
#include <WiFiUdp.h>
#endif
#if ESP32
#include <WiFi.h>
#include <WiFiClient.h>
#endif

#define CLIENTS max_avr_miners

#define CLIENT_CONNECT_EVERY 30000
#define CLIENT_TIMEOUT_CONNECTION 30000
#define CLIENT_TIMEOUT_REQUEST 100
#define I2C_SCAN_EMPTY_EVERY 1000
#define I2C_SCAN_ACTIVE_EVERY 10000

#define END_TOKEN  '\n'
#define SEP_TOKEN  ','

#define HASHRATE_FORCE false
#define HASHRATE_SPEED 258.0

bool clients_connected(byte i);
bool clients_stop(byte i);

String host = "";
int port = 6000;

void SetHostPort(String h, int p)
{
  host = h;
  port = p;
}

String SetHost(String h)
{
  host = h;
  return host;
}

int SetPort(int p)
{
  port = p;
  return port;
}

// State Machine
enum Duino_State
{
  DUINO_STATE_NONE,

  DUINO_STATE_VERSION_WAIT,

  DUINO_STATE_MOTD_REQUEST,
  DUINO_STATE_MOTD_WAIT,

  DUINO_STATE_JOB_REQUEST,
  DUINO_STATE_JOB_WAIT,

  DUINO_STATE_JOB_DONE_SEND,
  DUINO_STATE_JOB_DONE_WAIT,
};

WiFiClient clients[CLIENTS];
byte clientsWaitJob[CLIENTS];
int clientsShares[CLIENTS];
String clientsBuffer[CLIENTS];
unsigned long clientsTimes[CLIENTS];
unsigned long clientsTimeOut[CLIENTS];
byte clientsBadJob[CLIENTS];
bool clientsWireOnline[CLIENTS];
int clientsSlaveCount = 0;
unsigned long clientsLastScan = 0;

unsigned long clientsConnectTime = 0;
bool clientsMOTD = true;

void clients_scanSlaves(bool force)
{
  unsigned long interval = clientsSlaveCount == 0 ? I2C_SCAN_EMPTY_EVERY : I2C_SCAN_ACTIVE_EVERY;
  if (!force && millis() - clientsLastScan < interval) return;

  clientsLastScan = millis();
  int found = 0;
  for (byte i = 0; i < CLIENTS; i++)
  {
    bool online = wire_exists(i + 1);
    if (!online && clientsWireOnline[i] && clients_connected(i))
    {
      clients_stop(i);
    }
    clientsWireOnline[i] = online;
    if (online) found++;
  }
  clientsSlaveCount = found;
}

int clients_slaveCount()
{
  if (!miningMode_hasI2C()) return 0;
  clients_scanSlaves(false);
  return clientsSlaveCount;
}

bool clients_connected(byte i)
{
  return clients[i].connected();
}

bool clients_connect(byte i)
{
  if (clients[i].connected())
  {
    return true;
  }

  wire_readLine(i + 1);

  Serial.print("[" + String(i) + "]");
  Serial.println("Connecting to Duino-Coin server... " + String(host) + " " + String(port));

  ws_sendAll("[" + String(i) + "]" + "Connecting to Duino-Coin server... " + String(host) + " " + String(port));

  clients[i].setTimeout(30000);
  if (!clients[i].connect(host.c_str(), port))
  {
    Serial.print("[" + String(i) + "]");
    Serial.println("Connection failed.");
    UpdatePool();
    return false;
  }
  clients[i].setTimeout(100);

  clientsShares[i] = 0;
  clientsBadJob[i] = 0;
  clientsTimes[i] = millis();
  clientsBuffer[i] = "";
  clients_state(i, DUINO_STATE_VERSION_WAIT);
  return true;
}

void clients_state(byte i, byte state)
{
  clientsWaitJob[i] = state;
  clientsTimeOut[i] = millis();
}

bool clients_stop(byte i)
{
  clients_state(i, DUINO_STATE_NONE);
  clients[i].stop();
  return true;
}

int client_i = 0;

void clients_loop()
{
  if (!miningMode_hasI2C()) return;
  clients_scanSlaves(false);
  if (clientsSlaveCount == 0) return;

  if (clients_runEvery(clientsConnectTime))
  {
    clientsConnectTime = CLIENT_CONNECT_EVERY;
    for (client_i = 0; client_i < CLIENTS; client_i++)
    {
      int i = client_i;
      if (clientsWireOnline[i] && !clients_connect(i))
      {
        break;
      }
    }
  }

  for (client_i = 0; client_i < CLIENTS; client_i++)
  {
    int i = client_i;
    if (clientsWireOnline[i] && clients_connected(i))
    {

      for(int j = 0; j < 3; j++ )
      switch (clientsWaitJob[i])
      {
        case DUINO_STATE_VERSION_WAIT:
          clients_waitRequestVersion(i);
          break;
        case DUINO_STATE_JOB_REQUEST:
          clients_requestJob(i);
          break;
        case DUINO_STATE_JOB_WAIT:
          clients_waitRequestJob(i);
          break;
        case DUINO_STATE_JOB_DONE_SEND:
          clients_sendJobDone(i);
          break;
        case DUINO_STATE_JOB_DONE_WAIT:
          clients_waitFeedbackJobDone(i);
          break;
        case DUINO_STATE_MOTD_REQUEST:
          clients_requestMOTD(i);
          break;
        case DUINO_STATE_MOTD_WAIT:
          clients_waitMOTD(i);
          break;
      }

      if (millis() - clientsTimeOut[i] > CLIENT_TIMEOUT_CONNECTION)
      {
        Serial.println("[" + String(i) + "]" + " --------------- TIMEOUT ------------- ");
        ws_sendAll("[" + String(i) + "]" + " --------------- TIMEOUT ------------- ");
        clients_stop(i);
      }

    }
  }
}

void clients_waitMOTD(byte i)
{
  if (clients[i].available()) {
    String buffer = clients[i].readString();
    Serial.println("[" + String(i) + "]" + buffer);
    clientsWaitJob[i] = DUINO_STATE_JOB_REQUEST;
    clientsTimeOut[i] = millis();
  }
}

void clients_requestMOTD(byte i)
{
  Serial.print("[" + String(i) + "]");
  Serial.println("MOTD Request: " + String(ducouser));
  clients[i].print("MOTD");
  clientsWaitJob[i] = DUINO_STATE_MOTD_WAIT;
  clientsTimeOut[i] = millis();
}


void clients_waitRequestVersion(byte i)
{
  if (clients[i].available()) {
    String buffer = clients[i].readStringUntil(END_TOKEN);
    Serial.println("[" + String(i) + "]" + buffer);
    clients_state(i, DUINO_STATE_JOB_REQUEST);
    if (clientsMOTD) clients_state(i, DUINO_STATE_MOTD_REQUEST);
    clientsMOTD = false;
  }
}

void clients_requestJob(byte i)
{
  Serial.print("[" + String(i) + "]");
  Serial.println("Job Request: " + String(ducouser));
  String request = String("JOB,") + String(ducouser) + "," + JOB;
  if (strlen(minerKey) > 0) request += "," + String(minerKey);
  request += END_TOKEN;
  clients[i].print(request);
  clients_state(i, DUINO_STATE_JOB_WAIT);
}

void clients_waitRequestJob(byte i)
{
  String clientBuffer = clients_readData(i);
  if (clientBuffer.length() > 0)
  {
    Serial.print("[" + String(i) + "]");
    Serial.print("Job ");
    Serial.println(clientBuffer);

    // Not a Valid Job -> Request Again
    if (clientBuffer.indexOf(SEP_TOKEN) == -1)
    {
      clients_stop(i);
      return;
    }

    String hash = getValue(clientBuffer, SEP_TOKEN, 0);
    String job = getValue(clientBuffer, SEP_TOKEN, 1);
    unsigned int diff = getValue(clientBuffer, SEP_TOKEN, 2).toInt();

    Serial.print("[" + String(i) + "]");
    Serial.println("Job Receive: " + String(diff));

    wire_sendJob(i + 1, hash, job, diff);
    clients_state(i, DUINO_STATE_JOB_DONE_SEND);
  }
}

void clients_sendJobDone(byte i)
{
  String responseJob = wire_readLine(i + 1);
  if (responseJob.length() > 0)
  {
    ws_sendAll("[" + String(i) + "]" + responseJob);

    StreamString response;
    response.print(responseJob);

    int job = response.readStringUntil(SEP_TOKEN).toInt();
    int time = response.readStringUntil(SEP_TOKEN).toInt();
    String id = response.readStringUntil(END_TOKEN);
    float HashRate = job / (time * .000001f);

    if (HASHRATE_FORCE) // Force HashRate
    {
      Serial.print("[" + String(i) + "]");
      Serial.println("Force HashRate: " + String(HashRate, 2));
      HashRate = HASHRATE_SPEED + random(-50, 50) / 100.0;
    }

    if (id.length() > 0) id = SEP_TOKEN + id;

    String identifier = String(rigIdentifier) + " " + "[" + String(i + 1) + "]";

    clients[i].print(String(job) + SEP_TOKEN + String(HashRate, 2) + SEP_TOKEN + MINER + SEP_TOKEN + String(identifier) + id);

    Serial.print("[" + String(i) + "]");
    Serial.println(String(job) + SEP_TOKEN + String(HashRate, 2) + SEP_TOKEN+ MINER + SEP_TOKEN + String(identifier) + id);

    clients_state(i, DUINO_STATE_JOB_DONE_WAIT);
  }
}

void clients_waitFeedbackJobDone(byte i)
{
  String clientBuffer = clients_readData(i);
  if (clientBuffer.length() > 0)
  {
    unsigned long time = (millis() - clientsTimes[i]);
    clientsShares[i]++;
    int Shares = clientsShares[i];

    Serial.print("[" + String(i) + "]");
    Serial.println("Job " + clientBuffer  + ": Share #" + String(Shares) + " " + timeString(time));
    ws_sendAll("[" + String(i) + "]" + "Job " + clientBuffer  + ": Share #" + String(Shares) + " " + timeString(time));

    clients_state(i, DUINO_STATE_JOB_REQUEST);

    if (clientBuffer.indexOf("BAD") != -1)
    {
      if (clientsBadJob[i]++ > 3)
      {
        Serial.print("[" + String(i) + "]");
        Serial.println("BAD BAD BAD BAD");
        ws_sendAll("[" + String(i) + "]" + "BAD BAD BAD BAD");
        clients_stop(i);
      }
    }
    else
    {
      clientsBadJob[i] = 0;
    }
  }
}

String clients_string()
{
  if (!miningMode_hasI2C()) return "I2C off";
  clients_scanSlaves(false);

  int i = 0;
  String str;
  str += "I2C ";
  str += "[";
  str += " ";
  for (i = 0; i < CLIENTS; i++)
  {
    if (clientsWireOnline[i])
    {
      str += (i + 1);
      str += clients_connected(i) ? "." : "";
      str += " ";
    }
  }
  str += "]";
  return str;
}

String clients_show2()
{
  if (!miningMode_hasI2C()) return "I2C off\n";

  int i = 0;
  String str;
  for (i = 0; i < CLIENTS; i++)
  {
    str += String(i);
    str += " ";
  }
  str += "\n";
  for (i = 0; i < CLIENTS; i++)
  {
    str += clients[i].connected();
    str += " ";
  }
  str += "\n";

  for (i = 0; i < CLIENTS; i++)
  {
    str += clientsWireOnline[i];
    str += " ";
  }
  str += "\n";
  return str;
}

String clients_show()
{
  if (!miningMode_hasI2C()) return "I2C off\n";
  clients_scanSlaves(false);

  int i = 0;
  String str;
  str += client_i;
  str += "\n";
  for (i = 0; i < CLIENTS; i++)
  {
    str += "[" + String(i) + "]";
    str += " ";
    str += clients[i].connected();
    str += " ";

    if (clientsWireOnline[i])
    {
      str += "Connected ";
    }
    else
    {
      str += "Not ";
    }
    str += clientsWaitJob[i];
    str += " ";
    str += "B";
    str += clientsBadJob[i];

    str += "\n";
  }
  ws_sendAll(str);
  return str;
}

String timeString(unsigned long t) {
  String str = "";
  t /= 1000;
  int s = t % 60;
  int m = (t / 60) % 60;
  int h = (t / 3600);
  str += h;
  str += ":";
  if (m < 10) str += "0";
  str += m;
  str += ":";
  if (s < 10) str += "0";
  str += s;
  return str;
}


String clients_readData(byte i)
{
  String str = "";

  while (clients[i].available()) {
    char c = clients[i].read();
    if (c == END_TOKEN)
    {
      str = clientsBuffer[i];
      clientsBuffer[i] = "";
    }
    else
      clientsBuffer[i] += c;
  }

  if (clientsBuffer[i] != "")
  {
    if (millis() - clientsTimeOut[i] > CLIENT_TIMEOUT_REQUEST)
    {
      str = clientsBuffer[i];
      clientsBuffer[i] = "";
    }
  }

  return str;
}

// https://stackoverflow.com/questions/9072320/split-string-into-string-array
String getValue(String data, char separator, int index)
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

String waitForClientData(int i) {
  unsigned long previousMillis = millis();
  unsigned long interval = 10000;

  String buffer;
  while (clients[i].connected()) {
    if (clients[i].available()) {
      Serial.println(clients[i].available());
      buffer = clients[i].readStringUntil(END_TOKEN);
      if (buffer.length() == 1 && buffer[0] == END_TOKEN)
        buffer = "???\n"; // NOTE: Should never happen...
      if (buffer.length() > 0)
        break;
    }
    if (millis() - previousMillis >= interval) break;
    handleSystemEvents();
  }
  return buffer;
}

boolean clients_runEvery(unsigned long interval)
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
