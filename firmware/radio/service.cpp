// SPDX-License-Identifier: GPL-3.0-only
#include "app/internal.h"
#include "board/reference.h"
#include "core/radio_lifecycle.h"
#include <SPI.h>
#include <RadioLib.h>
namespace gw {
namespace {
class Port : public GatewayIO {
public:
  bool reserve(uint8_t i, uint16_t net, uint16_t node, uint64_t sess, uint32_t token) override {
    StorageJob j;
    j.kind = 1;
    j.index = i;
    j.network = net;
    j.node = node;
    j.session = sess;
    j.token = token;
    return xQueueSend(storageQ, &j, 0) == pdTRUE;
  }
  bool archive(uint8_t i, uint16_t net, uint16_t node, uint64_t sess, uint32_t corr,
               const uint8_t *p, uint64_t received) override {
    StorageJob j;
    j.kind = 2;
    j.index = i;
    j.network = net;
    j.node = node;
    j.session = sess;
    j.correlation = corr;
    j.received = received;
    memcpy(j.bytes, p, 446);
    return xQueueSend(storageQ, &j, 0) == pdTRUE;
  }
} port;
Gateway gateway(port);
SPIClass spi(FSPI);
Module module(board::cs, board::dio1, board::reset, board::busy, spi,
              SPISettings(2000000, MSBFIRST, SPI_MODE0));
SX1262 radio = &module;
volatile bool irq = false;
void IRAM_ATTR onIRQ() {
  irq = true;
}
using Phase = RadioPhase;
RadioLifecycle lifecycle;
Phase &phase = lifecycle.phase;
uint64_t txStart = 0, lastView = 0;
uint32_t txAir = 0, crcErrors = 0, driverErrors = 0;
int16_t code = 0;
bool initialized = false;
TxJob flight{};
uint8_t wire[MaxPacket]{};
Result results[8]{};
unsigned resultHead = 0;
void failure(int16_t c) {
  code = c;
  initialized = false;
  lifecycle.fail(mono());
  driverErrors++;
  logEvent("Radio initialization/operation failed; bounded 30s recovery");
}
void rx() {
  int16_t r = radio.startReceive();
  if (r)
    failure(r);
  else
    phase = Phase::Rx;
}
void configure() {
  initialized = false;
  irq = false;
  if (phase == Phase::Rx || phase == Phase::Tx)
    radio.standby();
  if (!gateway.config.enabled) {
    lifecycle.start(false, mono());
    code = 0;
    return;
  }
  spi.begin(board::sck, board::miso, board::mosi, board::cs);
  pinMode(board::busy, INPUT);
  pinMode(board::reset, OUTPUT);
  digitalWrite(board::reset, LOW);
  lifecycle.start(true, mono());
}
void begin() {
  radio.tcxoVoltage = board::tcxoVoltage;
  radio.resetOnStartup = false;
  module.spiConfig.timeout = 100;
  ConfigLoRa_t p;
  const Phy &phy = gateway.config.phy;
  p.frequency = phy.hz / 1000000.f;
  p.bandwidth = phy.bw;
  p.spreadingFactor = phy.sf;
  p.codingRate = phy.cr;
  p.syncWord = phy.sync;
  p.power = phy.dbm;
  p.preambleLength = phy.preamble;
  int16_t r = radio.begin(p);
  if (!r)
    r = radio.setDio2AsRfSwitch(board::dio2RfSwitch);
  if (!r)
    r = radio.setCurrentLimit(140);
  if (!r)
    r = radio.setCRC(2);
  if (!r)
    r = radio.invertIQ(false);
  if (r) {
    failure(r);
    return;
  }
  radio.setDio1Action(onIRQ);
  initialized = true;
  code = 0;
  rx();
  logEvent("SX1262 initialized; awaiting authenticated node traffic");
}
void publish() {
  View v;
  v.configured = true;
  v.boot = bootId;
  v.uptime = mono();
  v.radioEnabled = gateway.config.enabled;
  v.initialized = initialized;
  v.radioCode = code;
  v.network = gateway.config.network;
  v.gateway = gateway.config.gateway;
  v.revision = gateway.config.revision;
  v.phy = gateway.config.phy;
  v.counters = gateway.counters;
  v.automaticPolls = gateway.automaticPolls;
  v.automaticPollErrors = gateway.automaticPollErrors;
  v.radioStackMin = uxTaskGetStackHighWaterMark(nullptr);
  v.physicalCRC = crcErrors;
  v.driverErrors = driverErrors;
  v.txDepth = gateway.scheduler.depth;
  v.queueDrops = gateway.counters.queueDrops;
  v.expired = gateway.scheduler.expired;
  v.coalesced = gateway.scheduler.coalesced;
  v.dutyWait = gateway.scheduler.nextTx > v.uptime ? gateway.scheduler.nextTx - v.uptime : 0;
  strncpy(v.phase,
          phase == Phase::Disabled   ? "disabled"
          : phase == Phase::Tx       ? "transmitting"
          : phase == Phase::Rx       ? "receiving"
          : phase == Phase::Recovery ? "recovery"
                                     : "starting",
          23);
  StorageView s;
  if (xQueuePeek(storageViewQ, &s, 0) == pdTRUE) {
    v.storage = s.healthy;
    v.sequences = s.sequences;
    v.fsMounted = s.fsMounted;
    v.archiveHealthy = s.archiveHealthy;
    v.archiveCount = s.archiveCount;
    v.sessionSlots = s.sessionSlots;
    v.flashUsed = s.flashUsed;
    v.flashTotal = s.flashTotal;
    v.storageStackMin = s.stackMin;
    memcpy(v.archives, s.archives, sizeof(v.archives));
  }
  for (unsigned i = 0; i < 16; i++) {
    NodeView &x = v.nodes[i];
    const Node &entry = gateway.config.nodes[i];
    const NodeState &n = gateway.nodes[i];
    x.occupied = entry.occupied;
    x.enabled = entry.enabled;
    x.hasKey = hasKey(entry);
    x.network = entry.network;
    x.id = entry.id;
    x.revision = entry.revision;
    memcpy(x.name, entry.name, sizeof(x.name));
    memcpy(x.location, entry.location, sizeof(x.location));
    memcpy(x.notes, entry.notes, sizeof(x.notes));
    memcpy(x.firmware, n.firmware, sizeof(x.firmware));
    x.session = n.current.nonce;
    x.candidate = n.candidate.nonce;
    x.confirmed = n.confirmed;
    x.helloAcked = n.helloAcked;
    x.hasLive = n.hasLive;
    x.received = n.received;
    x.live = n.live;
    x.rssi = n.rssi;
    x.snr = n.snr;
    x.lastPacketReceived = n.lastPacketReceived;
    x.accepted = n.accepted;
    x.duplicates = n.duplicates;
    x.lastSequence = n.lastSequence;
    x.lastType = n.lastType;
    x.request = n.pending.state;
    x.correlation = n.pending.correlation;
  }
  Result result;
  while (xQueueReceive(resultQ, &result, 0) == pdTRUE) {
    results[resultHead] = result;
    resultHead = (resultHead + 1) % 8;
  }
  for (unsigned i = 0; i < 8; i++)
    v.results[i] = results[(resultHead + i) % 8];
  xQueueOverwrite(viewQ, &v);
  StatusSummary summary;
  summary.initialized = v.initialized;
  summary.radioCode = v.radioCode;
  memcpy(summary.phase, v.phase, sizeof(summary.phase));
  summary.counters = v.counters;
  summary.archiveCount = v.archiveCount;
  xQueueOverwrite(summaryQ, &summary);
}
} // namespace
void radioTask(void *) {
  Config cfg;
  bool ready = false;
  for (;;) {
    uint64_t now = mono();
    if (xQueueReceive(configQ, &cfg, 0) == pdTRUE) {
      gateway.configure(cfg);
      wipe(&cfg, sizeof(cfg));
      configure();
      ready = true;
      logEvent("Saved configuration applied; session reconfirmation required");
    }
    Completion done;
    while (xQueueReceive(completionQ, &done, 0) == pdTRUE) {
      if (done.kind == 1)
        gateway.sequenceReady(done.index, done.token, done.ok, done.sequence, now);
      else
        gateway.durable(done.index, done.session, done.correlation, done.ok);
    }
    Command command;
    if (xQueueReceive(radioCommandQ, &command, 0) == pdTRUE) {
      Result r;
      r.id = command.id;
      r.ok = command.expected == gateway.config.revision && initialized &&
             gateway.poll(command.index, command.type == CommandType::Checkpoint ? 1 : 0,
                          command.day, now);
      strncpy(r.message,
              r.ok ? "Request queued; result follows after node response"
                   : "Request rejected: revision, session, busy, radio, or queue",
              95);
      xQueueSend(resultQ, &r, 0);
    }
    const bool checkBusy = phase == Phase::ResetWait || phase == Phase::BusyWait;
    RadioAction action = lifecycle.step(now, checkBusy && digitalRead(board::busy));
    if (action == RadioAction::ReleaseReset) {
      digitalWrite(board::reset, HIGH);
    } else if (action == RadioAction::Initialize)
      begin();
    else if (action == RadioAction::BusyTimedOut)
      failure(RADIOLIB_ERR_SPI_CMD_TIMEOUT);
    else if (action == RadioAction::Restart)
      configure();
    if (initialized && (irq || digitalRead(board::dio1) == HIGH)) {
      irq = false;
      uint32_t flags = radio.getIrqFlags();
      if (phase == Phase::Tx && (flags & RADIOLIB_SX126X_IRQ_TX_DONE)) {
        int16_t r = radio.finishTransmit();
        gateway.transmitted(flight, r == 0, now, txAir);
        if (r)
          failure(r);
        else
          rx();
      } else if (phase == Phase::Rx && (flags & RADIOLIB_SX126X_IRQ_RX_DONE)) {
        size_t n = radio.getPacketLength();
        if (n >= 40 && n <= MaxPacket) {
          int16_t r = radio.readData(wire, n);
          if (!r) {
            Reject why = gateway.receive(wire, n, now, radio.getRSSI(), radio.getSNR());
            if (why != Reject::None)
              logEvent(reason(why));
          } else if (r == RADIOLIB_ERR_CRC_MISMATCH)
            crcErrors++;
          else
            driverErrors++;
        } else
          gateway.counters.malformed++;
        rx();
      } else {
        if (flags & RADIOLIB_SX126X_IRQ_CRC_ERR)
          crcErrors++;
        if (phase != Phase::Tx)
          rx();
      }
    }
    if (phase == Phase::Tx && RadioLifecycle::txExpired(now, txStart, txAir)) {
      radio.standby();
      gateway.transmitted(flight, false, now, txAir);
      failure(RADIOLIB_ERR_TX_TIMEOUT);
    }
    gateway.tick(now);
    if (phase == Phase::Rx && gateway.nextTx(now, flight)) {
      size_t n = encode(flight.frame, gateway.config.nodes[flight.node].key, wire);
      if (!n) {
        gateway.transmitted(flight, false, now, 1);
      } else {
        txAir = uint32_t((radio.getTimeOnAir(n) + 999) / 1000);
        if (!txAir)
          txAir = 1;
        int16_t r = radio.startTransmit(wire, n);
        txStart = now;
        if (!r)
          phase = Phase::Tx;
        else {
          gateway.transmitted(flight, false, now, txAir);
          failure(r);
        }
      }
    }
    if (ready && now - lastView >= 500) {
      lastView = now;
      publish();
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
} // namespace gw
