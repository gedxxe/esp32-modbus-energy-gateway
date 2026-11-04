// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <Arduino.h>
#include "core/gateway.h"
#include "storage/archive.h"
namespace gw {
enum class CommandType : uint8_t { SaveGateway, SaveNode, DeleteNode, Live, Checkpoint };
struct Command {
  uint32_t id = 0, expected = 0;
  CommandType type = CommandType::Live;
  uint8_t index = 0;
  Node node{};
  Phy phy{};
  bool enabled = false;
  uint16_t network = 1, gateway = 1;
  uint32_t day = 0;
};
struct Result {
  uint32_t id = 0;
  bool ok = false;
  char message[96]{};
};
struct NodeView {
  bool occupied = false, enabled = false, hasKey = false, helloAcked = false, hasLive = false;
  uint16_t network = 0, id = 0;
  uint32_t revision = 0;
  char name[33]{}, location[49]{}, notes[97]{}, firmware[22]{};
  uint64_t session = 0, candidate = 0, received = 0, confirmed = 0;
  uint64_t lastPacketReceived = 0;
  uint32_t accepted = 0, duplicates = 0, lastSequence = 0;
  uint8_t lastType = 0;
  Live live{};
  float rssi = 0, snr = 0;
  RequestState request = RequestState::Idle;
  uint32_t correlation = 0;
};
struct View {
  bool configured = false, radioEnabled = false, initialized = false, storage = false,
       sequences = false, fsMounted = false, archiveHealthy = false;
  uint16_t network = 1, gateway = 1;
  uint32_t revision = 0;
  Phy phy{};
  uint64_t boot = 0, uptime = 0, dutyWait = 0;
  int16_t radioCode = 0;
  char phase[24] = "disabled";
  Counters counters{};
  uint32_t automaticPolls = 0, automaticPollErrors = 0;
  uint32_t physicalCRC = 0, driverErrors = 0, queueDrops = 0, expired = 0, coalesced = 0;
  size_t txDepth = 0, archiveCount = 0, sessionSlots = 0;
  uint32_t flashUsed = 0, flashTotal = 0;
  uint32_t radioStackMin = 0, storageStackMin = 0;
  NodeView nodes[16]{};
  ArchiveInfo archives[64]{};
  Result results[8]{};
};
uint64_t mono();
struct StatusSummary {
  bool initialized = false;
  int16_t radioCode = 0;
  char phase[24]{};
  Counters counters{};
  size_t archiveCount = 0;
};
bool snapshotSummary(StatusSummary &s);
bool startRuntime();
bool submit(const Command &c);
bool snapshot(View &v);
bool readArchive(uint8_t index, uint8_t out[ArchiveBytes]);
void logEvent(const char *text);
String exportLog();
void startWeb();
void serviceWeb();
} // namespace gw
