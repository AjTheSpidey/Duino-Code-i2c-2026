/*
  DuinoCoin_Pool.ino
  created 31 07 2021
  Original code by: Luiz H. Cassettari
  Updated by: AjTheSpidey
*/

#if DUCO_ESP8266
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#endif
#if DUCO_ESP32
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#endif

const char * urlPool = "https://server.duinocoin.com/getPool";

String pool_getJsonString(String input, const char* key)
{
  String token = String("\"") + key + "\":\"";
  int start = input.indexOf(token);
  if (start < 0) return "";
  start += token.length();
  int end = input.indexOf('"', start);
  if (end < 0) return "";
  return input.substring(start, end);
}

int pool_getJsonInt(String input, const char* key)
{
  String token = String("\"") + key + "\":";
  int start = input.indexOf(token);
  if (start < 0) return 0;
  start += token.length();
  int end = input.indexOf(',', start);
  if (end < 0) end = input.indexOf('}', start);
  if (end < 0) return 0;
  return input.substring(start, end).toInt();
}

void UpdateHostPort(String input)
{
  // {"ip":"server.duinocoin.com","port":2812,"name":"Main server"}
  String name = pool_getJsonString(input, "name");
  String ip = pool_getJsonString(input, "ip");
  int port = pool_getJsonInt(input, "port");

  if (ip.length() == 0 || port == 0) {
    Serial.println("[ ]Pool JSON parse failed: " + input);
    return;
  }

  Serial.println("[ ]Update " + name + " " + ip + " " + String(port));
  SetHostPort(ip, port);
}

void UpdatePool()
{
  String input = httpGetString(urlPool);
  if (input == "") return;
  UpdateHostPort(input);
}

String httpGetString(String URL)
{
  String payload = "";
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (http.begin(client, URL))
  {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK)
    {
      payload = http.getString();
    }
    else
    {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }
  return payload;
}
