#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include "session.h"
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

constexpr char VERSION[] = "0.1.0";
constexpr uint8_t RELAY_PIN = 5, LED_PIN = 16;
ESP8266WebServer server(80);
RentalSession session;
String bootId;
bool configured = false, updating = false, otaWindow = false, otaStarted = false;
uint32_t otaSince = 0;

void relayTick() {
  session.tick(millis());
  // BA129 manual: GPIO5 HIGH = relay ON; GPIO16 LOW = LED ON.
  digitalWrite(RELAY_PIN, session.active && !updating ? HIGH : LOW);
  digitalWrite(LED_PIN, session.active && !updating ? LOW : HIGH);
}
void respond(int code, const char *message) {
  JsonDocument doc;
  doc["message"] = message;
  doc["boot_id"] = bootId;
  String json;
  serializeJson(doc, json);
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", json);
}
bool authorized() {
  if (server.header("Authorization") == String("Bearer ") + API_TOKEN) return true;
  respond(401, "unauthorized");
  return false;
}
void status() {
  relayTick();
  JsonDocument doc;
  doc["device_id"] = DEVICE_NAME;
  doc["firmware_version"] = VERSION;
  doc["boot_id"] = bootId;
  doc["uptime_seconds"] = millis() / 1000;
  doc["relay_on"] = session.active && !updating;
  doc["session_id"] = session.id;
  doc["revision"] = session.revision;
  doc["generation"] = session.generation;
  doc["remaining_seconds"] = session.remaining(millis());
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["ip"] = WiFi.localIP().toString();
  doc["ota_window_open"] = otaWindow;
  doc["ota_updating"] = updating;
  doc["free_heap"] = ESP.getFreeHeap();
  String json;
  serializeJson(doc, json);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}
bool body(JsonDocument &doc) {
  if (server.arg("plain").length() > 512) { respond(413, "body_too_large"); return false; }
  if (deserializeJson(doc, server.arg("plain"))) { respond(400, "invalid_json"); return false; }
  if (!doc["boot_id"].is<const char *>() || doc["boot_id"].as<String>() != bootId) {
    respond(409, "boot_id_mismatch_read_status"); return false;
  }
  return true;
}
void command(const char *op) {
  if (!authorized()) return;
  relayTick();
  if (updating || otaWindow) { respond(409, "maintenance_active"); return; }
  JsonDocument doc;
  if (!body(doc)) return;
  if (!doc["session_id"].is<const char *>() || !doc["revision"].is<uint32_t>() ||
      (!strcmp(op, "start") && !doc["expected_generation"].is<uint32_t>()) ||
      (strcmp(op, "stop") && !doc["remaining_seconds"].is<uint32_t>())) {
    respond(400, "invalid_fields"); return;
  }
  const char *id = doc["session_id"];
  // Keep identifiers simple for logs and integrations.
  if (!*id || strlen(id) > 64) { respond(400, "invalid_session_id"); return; }
  for (const char *p = id; *p; ++p) {
    if (!isalnum(static_cast<unsigned char>(*p)) && *p != '-' && *p != '_') {
      respond(400, "invalid_session_id"); return;
    }
  }
  uint32_t seconds = strcmp(op, "stop") ? doc["remaining_seconds"].as<uint32_t>() : 0;
  auto result = session.apply(op, id, doc["revision"].as<uint32_t>(), seconds, millis(),
                              doc["expected_generation"].as<uint32_t>());
  relayTick();
  if (result == RentalSession::INVALID) respond(400, "invalid_session");
  else if (result == RentalSession::CONFLICT) respond(409, "session_or_revision_conflict");
  else status();
}
void maintenance() {
  if (!authorized()) return;
  relayTick();
  JsonDocument doc;
  if (!body(doc)) return;
  if (session.active || updating) { respond(409, "device_busy"); return; }
  if (!doc["enabled"].is<bool>()) { respond(400, "enabled_required"); return; }
  otaWindow = doc["enabled"].as<bool>();
  otaSince = millis();
  if (otaWindow && !otaStarted) { ArduinoOTA.begin(); otaStarted = true; }
  if (!otaWindow && otaStarted) { ArduinoOTA.end(); otaStarted = false; }
  status();
}
void setup() {
  // Set the output latch LOW before enabling output. Boot-time electrical
  // behaviour before setup still needs verification on the actual board.
  digitalWrite(RELAY_PIN, LOW);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(57600);
  char boot[40];
  snprintf(boot, sizeof(boot), "%06x-%08x-%08x", ESP.getChipId(), ESP.random(), ESP.random());
  bootId = boot;
  configured = strlen(WIFI_SSID) > 0 && strlen(API_TOKEN) >= 32 && strlen(OTA_PASSWORD) >= 16;
  if (!configured) { Serial.println("Configuration missing: relay stays OFF. See README."); return; }
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(DEVICE_NAME);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  server.collectHeaders("Authorization");
  server.on("/status", HTTP_GET, [] { if (authorized()) status(); });
  server.on("/session/start", HTTP_POST, [] { command("start"); });
  server.on("/session/update", HTTP_POST, [] { command("update"); });
  server.on("/session/stop", HTTP_POST, [] { command("stop"); });
  server.on("/maintenance/ota", HTTP_POST, maintenance);
  server.onNotFound([] { if (authorized()) respond(404, "not_found"); });
  server.begin();
  ArduinoOTA.setHostname(DEVICE_NAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([] { updating = true; relayTick(); });
  ArduinoOTA.onError([](ota_error_t) { updating = false; otaWindow = false; relayTick(); });
}
void loop() {
  relayTick();
  if (!configured) { delay(1); return; }
  static bool logged = false;
  if (!logged && WiFi.status() == WL_CONNECTED) {
    logged = true;
    Serial.print("HTTP ready at "); Serial.println(WiFi.localIP());
  }
  if (otaWindow && !updating && uint32_t(millis() - otaSince) >= 300000) otaWindow = false;
  if (!otaWindow && otaStarted) { ArduinoOTA.end(); otaStarted = false; }
  server.handleClient();
  relayTick();
  if (otaStarted && otaWindow && !session.active) ArduinoOTA.handle();
  delay(1);
}
