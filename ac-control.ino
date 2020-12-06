// WiFi server libraries and settings
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#ifndef STASSID
#define STASSID "<WIFI SSID>"
#define STAPSK  "<WIFI PASSWORD>"
#endif
const char *ssid = STASSID;
const char *password = STAPSK;
ESP8266WebServer server(80);

// Json Library
#include <ArduinoJson.h>

// Filesystem library and settings
#include <FS.h>
int addr = 0;

// AC IR control libraries and settings
#include <Arduino.h>
#include <ToshibaDaiseikaiHeatpumpIR.h>
#ifndef ESP8266
IRSenderPWM irSender(4);
#else
IRSenderBitBang irSender(D2);
#endif
ToshibaDaiseikaiHeatpumpIR *heatpumpIR;

void setup(void) {
  Serial.begin(115200);

  bool success = SPIFFS.begin();
  if (success) {
    Serial.println("File system mounted with success");
  } else {
    Serial.println("Error mounting the file system");
    return;
  }

  Serial.println("");

  heatpumpIR = new ToshibaDaiseikaiHeatpumpIR();
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("Starting WiFi connection...");
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("");

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  Serial.println("");

  server.on(F("/"), []() {
    server.send(200, "text/plain", "hello from esp8266!");
  });

  server.on("/ac-set", handleSetRequest);
  server.on("/ac-status", handleStatusRequest);
  server.on("/ac-update", handleUpdateRequest);

  server.begin();
  Serial.println("HTTP server started");
}

void handleStatusRequest() {
  DynamicJsonDocument statusDoc(1024);
  String errorMessage = "";
  int httpStatus = 200;
  String json;

  File r = SPIFFS.open("/status.json", "r");
  String file_content = r.readString();
  auto error = deserializeJson(statusDoc, file_content);
  if (error) {
    errorMessage += String("deserializeJson() failed with code " + String(error.c_str()));
    httpStatus = 500;
  }
  r.close();

  json = "";
  serializeJsonPretty(statusDoc, json);
  Serial.println("Respoding:");
  Serial.println(json);

  json = "";
  serializeJsonPretty(statusDoc, json);
  server.send(httpStatus, "application/json", json);
}

void handleSetRequest() {
  DynamicJsonDocument requestParams(1024);
  String errorMessage = "";
  int httpStatus = 200;
  String json;
  
  if (server.arg("state") == ""){
    if (errorMessage.length() > 0) {
      errorMessage += "\n";
    }
    errorMessage += "State Argument not found";
  } else {
    requestParams["state"] = server.arg("state");
  }

  if (server.arg("mode") == ""){
    if (errorMessage.length() > 0) {
      errorMessage += "\n";
    }
    errorMessage += "Mode Argument not found";
  } else {
    requestParams["mode"] = server.arg("mode");
  }

  if (server.arg("fan") == ""){
    if (errorMessage.length() > 0) {
      errorMessage += "\n";
    }
    errorMessage += "Fan Argument not found";
  } else {
    requestParams["fan"] = server.arg("fan").toInt();
  }

  if (server.arg("temp") == ""){
    if (errorMessage.length() > 0) {
      errorMessage += "\n";
    }
    errorMessage += "Temp Argument not found";
  } else {
    requestParams["temp"] = server.arg("temp").toInt();
  }

  if (errorMessage.length() > 0) {
    requestParams["error"] = errorMessage;
    httpStatus = 400;
  } else {
    serializeJsonPretty(requestParams, json);
    Serial.println("Saving status:");
    Serial.println(json);
    File f = SPIFFS.open("/status.json", "w");
    auto error = serializeJsonPretty(requestParams, f);
    f.close();

    sendACCommand();
  }
  
  json = "";
  serializeJsonPretty(requestParams, json);
  server.send(httpStatus, "application/json", json);
}

void handleUpdateRequest() {
  DynamicJsonDocument statusDoc(1024);
  String errorMessage = "";
  int httpStatus = 200;
  String json;

  File r = SPIFFS.open("/status.json", "r");
  String file_content = r.readString();
  auto error = deserializeJson(statusDoc, file_content);
  if (error) {
    errorMessage += String("deserializeJson() failed with code " + String(error.c_str()));
    httpStatus = 500;
  }
  r.close();

  sendACCommand();

  json = "";
  serializeJsonPretty(statusDoc, json);
  server.send(httpStatus, "application/json", json);
}

void sendACCommand() {
  DynamicJsonDocument statusDoc(1024);
  File r = SPIFFS.open("/status.json", "r");
  String file_content = r.readString();
  auto error = deserializeJson(statusDoc, file_content);
  r.close();

  String json;
  serializeJsonPretty(statusDoc, json);
  Serial.println("Sending:");
  Serial.println(json);

  String acState = statusDoc["state"];
  String acMode = statusDoc["mode"];
  int acFan = statusDoc["fan"];
  int acTemp = statusDoc["temp"];

  int sendState = POWER_OFF;
  if (acState == "on") {
    sendState = POWER_ON;
  } else if (acState == "off") {
    sendState = POWER_OFF;
  }

  int sendMode = MODE_AUTO;
  if (acMode == "auto") {
    sendMode = MODE_AUTO;
  } else if (acMode == "heat") {
    sendMode = MODE_HEAT;
  } else if (acMode == "cool") {
    sendMode = MODE_COOL;
  } else if (acMode == "dry") {
    sendMode = MODE_DRY;
  } else if (acMode == "fan") {
    sendMode = MODE_FAN;
  }

  int sendFan = FAN_AUTO;
  if (acFan == 0) {
    sendFan = FAN_AUTO;
  } else if (acFan == 1) {
    sendFan = FAN_1;
  } else if (acFan == 2) {
    sendFan = FAN_2;
  } else if (acFan == 3) {
    sendFan = FAN_3;
  } else if (acFan == 4) {
    sendFan = FAN_4;
  } else if (acFan == 5) {
    sendFan = FAN_5;
  } else if (acFan == 6) {
    sendFan = FAN_SILENT;
  }

  int sendTemp = 24;
  if (acTemp >= 17 && acTemp <= 30) {
    sendTemp = acTemp;
  }

  Serial.println("Sending:");
  Serial.print("state: ");
  Serial.println(sendState);
  Serial.print("mode: ");
  Serial.println(sendMode);
  Serial.print("fan: ");
  Serial.println(sendFan);
  Serial.print("temp: ");
  Serial.println(sendTemp);
  heatpumpIR->send(irSender, sendState, sendMode, sendFan, sendTemp, VDIR_AUTO, HDIR_AUTO);
}

void loop(void) {
  server.handleClient();
}
