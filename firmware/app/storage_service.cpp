// SPDX-License-Identifier: GPL-3.0-only
#include "internal.h"
#include "storage/sequences.h"
#include <Preferences.h>
#include <LittleFS.h>
namespace gw {
namespace {
class NVS : public Backend {
public:
  Preferences prefs;
  size_t size(const char *k) override {
    return prefs.getBytesLength(k);
  }
  size_t read(const char *k, uint8_t *p, size_t n) override {
    return prefs.getBytes(k, p, n);
  }
  size_t write(const char *k, const uint8_t *p, size_t n) override {
    return prefs.putBytes(k, p, n);
  }
} nvs;
class Files : public Backend {
public:
  size_t size(const char *k) override {
    String path = "/";
    path += k;
    File f = LittleFS.open(path, "r");
    return f ? f.size() : 0;
  }
  size_t read(const char *k, uint8_t *p, size_t n) override {
    String path = "/";
    path += k;
    File f = LittleFS.open(path, "r");
    return f ? f.read(p, n) : 0;
  }
  size_t write(const char *k, const uint8_t *p, size_t n) override {
    String path = "/";
    path += k;
    File f = LittleFS.open(path, "w");
    if (!f)
      return 0;
    size_t z = f.write(p, n);
    f.flush();
    f.close();
    return z;
  }
} files;
Banks configBanks(nvs, "cfg_a", "cfg_b", 1), seqBanks(nvs, "seq_a", "seq_b", 2);
Sequences seq(seqBanks);
Archives archives(files);
Config active{};
uint8_t configWire[ConfigBytes]{};
bool configHealthy = false, fsMounted = false;
void result(uint32_t id, bool ok, const char *text) {
  Result r;
  r.id = id;
  r.ok = ok;
  strncpy(r.message, text, sizeof(r.message) - 1);
  xQueueSend(resultQ, &r, 0);
  logEvent(text);
}
void publish() {
  StorageView s;
  s.healthy = configHealthy;
  s.sequences = seq.healthy;
  s.fsMounted = fsMounted;
  s.archiveHealthy = fsMounted && archives.healthy;
  s.sessionSlots = seq.used;
  s.archiveCount = archives.count;
  s.stackMin = uxTaskGetStackHighWaterMark(nullptr);
  if (fsMounted) {
    s.flashUsed = LittleFS.usedBytes();
    s.flashTotal = LittleFS.totalBytes();
  }
  memcpy(s.archives, archives.entries, sizeof(s.archives));
  xQueueOverwrite(storageViewQ, &s);
}
} // namespace
void storageTask(void *) {
  bool opened = nvs.prefs.begin("es_gateway", false, "gw_nvs");
  Load load = opened ? configBanks.load(configWire, sizeof(configWire)) : Load::Corrupt;
  configHealthy = load != Load::Corrupt &&
                  (load == Load::Empty || decodeConfig(configWire, sizeof(configWire), active));
  bool seqOK = opened && seq.load(load == Load::Empty && configHealthy);
  if (!configHealthy)
    active = Config{};
  if (!seqOK)
    logEvent("Sequence storage unsafe; POLL blocked. Follow local recovery guide.");
  fsMounted = LittleFS.begin(false, "/littlefs", 10, "archives");
  if (fsMounted) {
    archives.load();
  } else
    logEvent("Archive filesystem unavailable; no automatic format. Radio ACK != durability.");
  xQueueOverwrite(configQ, &active);
  publish();
  for (;;) {
    StorageJob job;
    if (xQueueReceive(storageQ, &job, 0) == pdTRUE) {
      if (job.kind == 3) {
        ArchiveReply r;
        r.index = job.index;
        r.ok = fsMounted && archives.read(job.index, r.bytes);
        xQueueOverwrite(archiveReplyQ, &r);
      } else {
        Completion done;
        done.kind = job.kind;
        done.index = job.index;
        done.session = job.session;
        done.token = job.token;
        done.correlation = job.correlation;
        if (job.kind == 1)
          done.ok = seq.take(job.network, job.node, job.session, done.sequence);
        else if (job.kind == 2)
          done.ok = fsMounted && archives.save(job.network, job.node, job.session, job.correlation,
                                               job.bytes, bootId, job.received);
        xQueueSend(completionQ, &done, pdMS_TO_TICKS(100));
      }
      wipe(&job, sizeof(job));
      publish();
    }
    Command c;
    if (xQueueReceive(commandQ, &c, pdMS_TO_TICKS(10)) == pdTRUE) {
      if (c.type == CommandType::Live || c.type == CommandType::Checkpoint) {
        if (xQueueSend(radioCommandQ, &c, 0) != pdTRUE)
          result(c.id, false, "Radio command queue full");
      } else if (!configHealthy)
        result(c.id, false, "Configuration storage corrupt; recover locally before edits");
      else if (c.expected != active.revision)
        result(c.id, false, "Revision conflict; reload settings");
      else {
        Config next = active;
        bool ok = true;
        if (c.type == CommandType::SaveGateway) {
          next.phy = c.phy;
          next.enabled = c.enabled;
          next.gateway = c.gateway;
          next.network = c.network;
        } else if (c.index >= 16)
          ok = false;
        else if (c.type == CommandType::DeleteNode)
          next.nodes[c.index] = Node{};
        else {
          Node entry = c.node;
          entry.occupied = true;
          entry.revision = active.nodes[c.index].revision + 1;
          if (!entry.revision)
            ok = false;
          if (!hasKey(entry) && active.nodes[c.index].occupied &&
              entry.network == active.nodes[c.index].network &&
              entry.id == active.nodes[c.index].id)
            memcpy(entry.key, active.nodes[c.index].key, 32);
          next.nodes[c.index] = entry;
        }
        if (active.revision == UINT32_MAX)
          ok = false;
        else
          next.revision = active.revision + 1;
        ok = ok && validConfig(next);
        if (ok) {
          encodeConfig(next, configWire);
          ok = configBanks.save(configWire, sizeof(configWire));
        }
        if (ok) {
          active = next;
          xQueueOverwrite(configQ, &active);
          result(c.id, true, "Saved and queued for radio apply; sessions will reconfirm");
        } else
          result(c.id, false,
                 "Save failed: invalid settings or storage verification; active settings retained");
        wipe(&next, sizeof(next));
      }
      wipe(&c, sizeof(c));
      publish();
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
} // namespace gw
