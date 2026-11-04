// SPDX-License-Identifier: GPL-3.0-only
#include "app/runtime.h"
#include "assets.h"
#include <WiFi.h>
#include <esp_http_server.h>
#include <ArduinoJson.h>
#include <esp_system.h>
namespace gw {
namespace {
httpd_handle_t server = nullptr;
char accessCode[33]{};
uint32_t commandId = 0;
View view;
bool webReady = false;
esp_err_t text(httpd_req_t *req, int status, const char *body) {
  httpd_resp_set_status(req, status == 401   ? "401 Unauthorized"
                             : status == 503 ? "503 Service Unavailable"
                             : status == 202 ? "202 Accepted"
                             : status == 413 ? "413 Payload Too Large"
                             : status == 400 ? "400 Bad Request"
                                             : "200 OK");
  httpd_resp_set_type(req, "text/plain; charset=utf-8");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
  return httpd_resp_sendstr(req, body);
}
bool authorized(httpd_req_t *req) {
  char token[40]{};
  if (httpd_req_get_hdr_value_len(req, "X-Commissioning-Token") != 32 ||
      httpd_req_get_hdr_value_str(req, "X-Commissioning-Token", token, sizeof(token)) != ESP_OK ||
      !equal(reinterpret_cast<const uint8_t *>(token),
             reinterpret_cast<const uint8_t *>(accessCode), 32)) {
    text(req, 401, "Commissioning access code required");
    return false;
  }
  return true;
}
esp_err_t json(httpd_req_t *req, JsonDocument &doc, int status = 200) {
  String out;
  serializeJson(doc, out);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
  if (status == 202)
    httpd_resp_set_status(req, "202 Accepted");
  return httpd_resp_send(req, out.c_str(), out.length());
}
void hex64(uint64_t n, char *out) {
  snprintf(out, 17, "%016llx", (unsigned long long)n);
}
esp_err_t index(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
  httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
  httpd_resp_set_hdr(
      req, "Content-Security-Policy",
      "default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src "
      "'self'; img-src 'self' blob:; object-src 'none'; base-uri 'none'; frame-ancestors 'none'");
  return httpd_resp_send(req, reinterpret_cast<const char *>(DashboardGzip), DashboardSize);
}
esp_err_t status(httpd_req_t *req) {
  if (!authorized(req))
    return ESP_OK;
  if (!snapshot(view))
    return text(req, 503, "Gateway starting");
  JsonDocument d;
  d["revision"] = view.revision;
  d["network"] = view.network;
  d["gateway"] = view.gateway;
  d["radioEnabled"] = view.radioEnabled;
  d["initialized"] = view.initialized;
  d["radioCode"] = view.radioCode;
  d["phase"] = view.phase;
  char hex[17];
  hex64(view.boot, hex);
  d["boot"] = hex;
  d["uptime"] = view.uptime;
  d["dutyWait"] = view.dutyWait;
  d["storage"] = view.storage;
  d["sequences"] = view.sequences;
  d["fsMounted"] = view.fsMounted;
  d["archiveHealthy"] = view.archiveHealthy;
  d["archiveCount"] = view.archiveCount;
  d["sessionSlots"] = view.sessionSlots;
  d["flashUsed"] = view.flashUsed;
  d["flashTotal"] = view.flashTotal;
  d["physicalCRC"] = view.physicalCRC;
  d["driverErrors"] = view.driverErrors;
  d["freeHeap"] = ESP.getFreeHeap();
  d["minimumHeap"] = ESP.getMinFreeHeap();
  d["radioStackMin"] = view.radioStackMin;
  d["storageStackMin"] = view.storageStackMin;
  d["httpStackMin"] = uxTaskGetStackHighWaterMark(nullptr);
  d["queueDrops"] = view.queueDrops;
  d["txDepth"] = view.txDepth;
  d["expired"] = view.expired;
  d["coalesced"] = view.coalesced;
  d["utc"] = nullptr;
  d["ackAirMs"] = airtimeUs(view.phy, 41) / 1000.;
  d["telemetryAirMs"] = airtimeUs(view.phy, 72) / 1000.;
  JsonObject p = d["phy"].to<JsonObject>();
  p["hz"] = view.phy.hz;
  p["bw"] = view.phy.bw;
  p["preamble"] = view.phy.preamble;
  p["sf"] = view.phy.sf;
  p["cr"] = view.phy.cr;
  p["sync"] = view.phy.sync;
  p["dbm"] = view.phy.dbm;
  JsonObject c = d["counters"].to<JsonObject>();
#define COUNT(name) c[#name] = view.counters.name
  COUNT(rx);
  COUNT(accepted);
  COUNT(tx);
  COUNT(acks);
  COUNT(duplicates);
  COUNT(auth);
  COUNT(malformed);
  COUNT(unknown);
  COUNT(disabled);
  COUNT(address);
  COUNT(replay);
  COUNT(session);
  COUNT(correlation);
  COUNT(timeouts);
  COUNT(queueDrops);
  COUNT(recordErrors);
  COUNT(validated);
  COUNT(durable);
  COUNT(lateAck);
  COUNT(storageErrors);
#undef COUNT
  JsonArray nodes = d["nodes"].to<JsonArray>();
  d["inventoryCapacity"] = InventoryCapacity;
  for (unsigned i = 0; i < 16; i++) {
    const NodeView &n = view.nodes[i];
    if (!n.occupied)
      continue;
    JsonObject o = nodes.add<JsonObject>();
    o["index"] = i;
    o["network"] = n.network;
    o["id"] = n.id;
    o["enabled"] = n.enabled;
    o["hasKey"] = n.hasKey;
    o["revision"] = n.revision;
    o["name"] = n.name;
    o["location"] = n.location;
    o["notes"] = n.notes;
    o["firmware"] = n.firmware;
    if (n.session) {
      hex64(n.session, hex);
      o["session"] = hex;
    } else
      o["session"] = nullptr;
    if (n.candidate) {
      hex64(n.candidate, hex);
      o["candidate"] = hex;
    } else
      o["candidate"] = nullptr;
    o["helloAcked"] = n.helloAcked;
    o["hasLive"] = n.hasLive;
    o["receiveAge"] = n.hasLive ? view.uptime - n.received : 0;
    o["request"] = requestName(n.request);
    o["correlation"] = n.correlation;
    o["rssi"] = n.rssi;
    o["snr"] = n.snr;
    o["accepted"] = n.accepted;
    o["duplicates"] = n.duplicates;
    o["lastSequence"] = n.lastSequence;
    o["lastType"] = n.lastType;
    if (n.accepted || n.duplicates)
      o["packetAge"] = view.uptime - n.lastPacketReceived;
    else
      o["packetAge"] = nullptr;
    JsonObject l = o["live"].to<JsonObject>();
    l["flags"] = n.live.flags;
    l["correlation"] = n.live.correlation;
    l["utc"] = n.live.utc;
    l["sampleAge"] = n.live.sampleAge;
    l["day"] = n.live.day;
    l["revision"] = n.live.revision;
    l["profile"] = n.live.profile;
    l["meterId"] = n.live.meterId;
    if (n.hasLive && (n.live.flags & 1) && std::isfinite(n.live.kwh))
      l["kwh"] = n.live.kwh;
    else
      l["kwh"] = nullptr;
  }
  JsonArray results = d["results"].to<JsonArray>();
  for (const auto &r : view.results)
    if (r.id) {
      JsonObject o = results.add<JsonObject>();
      o["id"] = r.id;
      o["ok"] = r.ok;
      o["message"] = r.message;
    }
  return json(req, d);
}
bool number(JsonVariantConst v, uint32_t lo, uint32_t hi, uint32_t &out) {
  if (!v.is<uint32_t>())
    return false;
  uint32_t n = v.as<uint32_t>();
  if (n < lo || n > hi)
    return false;
  out = n;
  return true;
}
bool stringField(JsonVariantConst v, char *out, size_t capacity) {
  if (!v.is<const char *>())
    return false;
  const char *str = v.as<const char *>();
  size_t n = strlen(str);
  if (n >= capacity)
    return false;
  for (size_t i = 0; i < n; i++)
    if (uint8_t(str[i]) < 32)
      return false;
  memcpy(out, str, n + 1);
  return true;
}
int digit(char c) {
  return c >= '0' && c <= '9'   ? c - '0'
         : c >= 'a' && c <= 'f' ? c - 'a' + 10
         : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                : -1;
}
bool parseNode(JsonVariantConst v, Node &n) {
  uint32_t a, b;
  if (!number(v["network"], 1, 65535, a) || !number(v["id"], 1, 65534, b) ||
      !v["enabled"].is<bool>())
    return false;
  n.network = uint16_t(a);
  n.id = uint16_t(b);
  n.enabled = v["enabled"];
  if (!stringField(v["name"], n.name, sizeof(n.name)) ||
      !stringField(v["location"], n.location, sizeof(n.location)) ||
      !stringField(v["notes"], n.notes, sizeof(n.notes)))
    return false;
  const char *key = v["key"] | "";
  size_t len = strlen(key);
  if (len && len != 64)
    return false;
  if (len)
    for (unsigned i = 0; i < 32; i++) {
      int h = digit(key[i * 2]), l = digit(key[i * 2 + 1]);
      if (h < 0 || l < 0)
        return false;
      n.key[i] = uint8_t(h * 16 + l);
    }
  bool testKey = true;
  for (unsigned i = 0; i < 32; i++)
    testKey &= n.key[i] == i;
  return !testKey && (!len || hasKey(n));
}
esp_err_t command(httpd_req_t *req) {
  if (!authorized(req))
    return ESP_OK;
  if (req->content_len == 0 || req->content_len > 1536)
    return text(req, 413, "Command body limit is 1536 bytes");
  char type[48]{};
  if (httpd_req_get_hdr_value_str(req, "Content-Type", type, sizeof(type)) != ESP_OK ||
      strncmp(type, "application/json", 16))
    return text(req, 400, "JSON required");
  char body[1537]{};
  size_t pos = 0;
  const uint64_t started = mono();
  while (pos < req->content_len) {
    if (mono() - started > 3000) {
      wipe(body, sizeof(body));
      return text(req, 400, "Command receive deadline exceeded");
    }
    int n = httpd_req_recv(req, body + pos, req->content_len - pos);
    if (n <= 0) {
      wipe(body, sizeof(body));
      return text(req, 400, "Incomplete command");
    }
    pos += size_t(n);
  }
  JsonDocument d;
  DeserializationError e = deserializeJson(d, body, pos, DeserializationOption::NestingLimit(4));
  wipe(body, sizeof(body));
  if (e)
    return text(req, 400, "Invalid JSON");
  Command c;
  uint32_t n = 0;
  bool ok = number(d["expected"], 0, UINT32_MAX, c.expected);
  const char *action = d["action"] | "";
  if (!strcmp(action, "saveGateway")) {
    c.type = CommandType::SaveGateway;
    ok = ok && number(d["network"], 1, 65535, n);
    c.network = uint16_t(n);
    ok = ok && number(d["gateway"], 1, 65534, n);
    c.gateway = uint16_t(n);
    ok = ok && d["enabled"].is<bool>();
    c.enabled = d["enabled"] | false;
    ok = ok && number(d["hz"], 803000000, 930000000, c.phy.hz);
    ok = ok && number(d["bw"], 125, 500, n);
    c.phy.bw = uint16_t(n);
    ok = ok && number(d["preamble"], 8, 64, n);
    c.phy.preamble = uint16_t(n);
    ok = ok && number(d["sf"], 7, 12, n);
    c.phy.sf = uint8_t(n);
    ok = ok && number(d["cr"], 5, 8, n);
    c.phy.cr = uint8_t(n);
    ok = ok && number(d["sync"], 0, 255, n);
    c.phy.sync = uint8_t(n);
    ok = ok && number(d["dbm"], 0, 22, n);
    c.phy.dbm = int8_t(n);
    ok = ok && validPhy(c.phy);
  } else {
    ok = ok && number(d["index"], 0, 15, n);
    c.index = uint8_t(n);
    if (!strcmp(action, "saveNode")) {
      c.type = CommandType::SaveNode;
      ok = ok && parseNode(d["node"], c.node);
    } else if (!strcmp(action, "deleteNode"))
      c.type = CommandType::DeleteNode;
    else if (!strcmp(action, "live"))
      c.type = CommandType::Live;
    else if (!strcmp(action, "checkpoint")) {
      c.type = CommandType::Checkpoint;
      ok = ok && number(d["day"], 0, MaxUTC / 86400, c.day) && (c.day == 0 || validDay(c.day));
    } else
      ok = false;
  }
  if (!ok) {
    wipe(&c, sizeof(c));
    d.clear();
    return text(req, 400, "Invalid command fields");
  }
  c.id = ++commandId;
  if (!c.id)
    c.id = ++commandId;
  uint32_t id = c.id;
  ok = submit(c);
  wipe(&c, sizeof(c));
  d.clear();
  if (!ok)
    return text(req, 503, "Command queue full; retry later");
  JsonDocument r;
  r["id"] = id;
  r["state"] = "queued";
  return json(req, r, 202);
}
esp_err_t logs(httpd_req_t *req) {
  if (!authorized(req))
    return ESP_OK;
  String s = exportLog();
  httpd_resp_set_type(req, "text/plain; charset=utf-8");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, s.c_str(), s.length());
}
esp_err_t archiveList(httpd_req_t *req) {
  if (!authorized(req))
    return ESP_OK;
  if (!snapshot(view))
    return text(req, 503, "Gateway starting");
  JsonDocument d;
  JsonArray a = d["archives"].to<JsonArray>();
  char hex[17];
  for (unsigned i = 0; i < 64; i++) {
    const ArchiveInfo &x = view.archives[i];
    if (!x.used)
      continue;
    JsonObject o = a.add<JsonObject>();
    o["index"] = i;
    o["network"] = x.network;
    o["node"] = x.node;
    o["day"] = x.day;
    o["revision"] = x.revision;
    hex64(x.boot, hex);
    o["boot"] = hex;
    o["received"] = x.received;
  }
  return json(req, d);
}
esp_err_t archiveRecord(httpd_req_t *req) {
  if (!authorized(req))
    return ESP_OK;
  char query[32]{}, slot[8]{};
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
      httpd_query_key_value(query, "slot", slot, sizeof(slot)) != ESP_OK)
    return text(req, 400, "Archive slot required");
  char *end = nullptr;
  unsigned long i = strtoul(slot, &end, 10);
  if (!slot[0] || *end || i >= 64)
    return text(req, 400, "Invalid archive slot");
  uint8_t bytes[ArchiveBytes];
  if (!readArchive(uint8_t(i), bytes))
    return text(req, 503, "Archive read unavailable; retry");
  Record r;
  if (!decodeRecord(bytes + 48, 446, r))
    return text(req, 503, "Archive validation failed");
  JsonDocument d;
  d["schema"] = 3;
  d["network"] = u16(bytes);
  d["node"] = u16(bytes + 2);
  d["day"] = r.day;
  d["revision"] = r.revision;
  d["generation"] = r.generation;
  char h[17];
  hex64(u64(bytes + 8), h);
  d["receiveBoot"] = h;
  d["receiveMonotonicMs"] = u64(bytes + 16);
  d["gatewayUTC"] = nullptr;
  JsonArray points = d["points"].to<JsonArray>();
  for (unsigned j = 0; j < 10; j++) {
    const Point &p = r.points[j];
    JsonObject o = points.add<JsonObject>();
    o["minuteWIB"] = CaptureMinutes[j];
    o["status"] = p.status == 1 ? "recorded" : p.status == 2 ? "missed" : "waiting";
    if (p.status != 1) {
      o["kwh"] = nullptr;
      continue;
    }
    o["kwh"] = p.energy;
    o["startedUTC"] = p.started;
    o["capturedUTC"] = p.captured;
    o["profileId"] = p.profile;
    o["savedMeterId"] = p.meterId;
    o["modbusSlaveId"] = p.slave;
    o["baud"] = p.baud;
    o["parity"] = String(p.parity);
    o["direction"] = p.direction;
    o["ptNumerator"] = p.ptNum;
    o["ptDenominator"] = p.ptDen;
    o["ct"] = p.ct;
    o["raw"] = p.raw;
    o["generation"] = p.generation;
  }
  return json(req, d);
}
} // namespace
void startWeb() {
  // Enable RF entropy before generating boot commissioning credentials.
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  uint8_t entropy[40];
  esp_fill_random(entropy, sizeof(entropy));
  char password[17];
  constexpr char alphabet[] = "23456789abcdefghjkmnpqrstuvwxyz";
  for (unsigned i = 0; i < 16; i++) {
    password[i] = alphabet[entropy[i] % (sizeof(alphabet) - 1)];
    snprintf(accessCode + 2 * i, 3, "%02x", entropy[i + 16]);
  }
  password[16] = 0;
  commandId = u32(entropy + 32);
  wipe(entropy, sizeof(entropy));
  char ssid[32];
  snprintf(ssid, sizeof(ssid), "Energy-Gateway-%04x", unsigned(ESP.getEfuseMac() & 0xffff));
  if (!WiFi.softAP(ssid, password, 1, false, 2)) {
    Serial.println("OUTPUT: AP startup failed");
    return;
  }
  Serial.printf("OUTPUT: AP SSID: %s\nOUTPUT: AP password (this boot): %s\nOUTPUT: Open "
                "http://192.168.4.1\nOUTPUT: Commissioning access code (this boot): %s\n",
                ssid, password, accessCode);
  wipe(password, sizeof(password));
  httpd_config_t c = HTTPD_DEFAULT_CONFIG();
  c.stack_size = 12288;
  c.task_priority = 1; // Radio owner (priority 2) can preempt dashboard serialization.
  c.core_id = tskNO_AFFINITY;
  c.max_open_sockets = 2;
  c.max_uri_handlers = 8;
  c.lru_purge_enable = true;
  c.recv_wait_timeout = 2;
  c.send_wait_timeout = 2;
  if (httpd_start(&server, &c) != ESP_OK) {
    Serial.println("OUTPUT: HTTP startup failed");
    return;
  }
  httpd_uri_t routes[] = {{"/", HTTP_GET, index, nullptr},
                          {"/api/status", HTTP_GET, status, nullptr},
                          {"/api/command", HTTP_POST, command, nullptr},
                          {"/api/logs", HTTP_GET, logs, nullptr},
                          {"/api/archives", HTTP_GET, archiveList, nullptr},
                          {"/api/archive", HTTP_GET, archiveRecord, nullptr}};
  for (auto &route : routes)
    httpd_register_uri_handler(server, &route);
  webReady = true;
}
void serviceWeb() {
  static char line[32]{};
  static size_t used = 0;
  while (Serial.available()) {
    char ch = char(Serial.read());
    if (ch == '\r' || ch == '\n') {
      line[used] = 0;
      if (!strcmp(line, "help"))
        Serial.println("OUTPUT: Serial Monitor INPUT + Enter: status | help. Configure using "
                       "authorized local dashboard buttons.");
      else if (!strcmp(line, "status")) {
        StatusSummary v;
        if (snapshotSummary(v))
          Serial.printf("OUTPUT: phase=%s initialized=%d driver=%d accepted=%lu auth=%lu "
                        "replay=%lu archive=%u UTC=unavailable\n",
                        v.phase, v.initialized, v.radioCode, (unsigned long)v.counters.accepted,
                        (unsigned long)v.counters.auth, (unsigned long)v.counters.replay,
                        unsigned(v.archiveCount));
      }
      used = 0;
    } else if (used < 31)
      line[used++] = ch;
  }
  if (!webReady)
    delay(10);
}
} // namespace gw
