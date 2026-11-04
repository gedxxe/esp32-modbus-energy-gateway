// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "config.h"
#include "protocol.h"
#include "checkpoint.h"
#include "scheduler.h"
#include "refresh_policy.h"
namespace gw {
constexpr size_t ReplayWindow = 16;
struct Replay {
  uint32_t high = 0;
  uint64_t bits = 0;
  uint8_t tags[ReplayWindow][16]{};
  // 0=new, 1=exact duplicate, -1=old or conflicting reuse. No mutation until accept().
  int check(uint32_t seq, const uint8_t tag[16]) const;
  void accept(uint32_t seq, const uint8_t tag[16]);
};
enum class RequestState : uint8_t {
  Idle,
  Reserving,
  Queued,
  Waiting,
  Validated,
  Durable,
  Unavailable,
  Error
};
const char *requestName(RequestState s);
struct Session {
  uint64_t nonce = 0;
  Replay replay{};
};
struct Pending {
  RequestState state = RequestState::Idle;
  uint64_t session = 0, deadline = 0;
  uint32_t correlation = 0, token = 0, day = 0;
  uint8_t mode = 0;
  bool challenge = false;
};
struct NodeState {
  Session current{}, candidate{};
  Live live{}, lastValid{};
  bool hasLive = false, hasLastValid = false, helloAcked = false;
  uint64_t lastValidReceived = 0;
  uint64_t received = 0, lastChallenge = 0, confirmed = 0;
  uint64_t lastPacketReceived = 0;
  uint32_t accepted = 0, duplicates = 0;
  uint32_t lastSequence = 0;
  uint8_t lastType = 0;
  float rssi = 0, snr = 0;
  Pending pending{};
  char firmware[22]{};
};
struct Counters {
  uint32_t rx = 0, accepted = 0, tx = 0, acks = 0, duplicates = 0, auth = 0, malformed = 0,
           unknown = 0, disabled = 0, address = 0, replay = 0, session = 0, correlation = 0,
           timeouts = 0, queueDrops = 0, recordErrors = 0, validated = 0, durable = 0, lateAck = 0,
           storageErrors = 0;
};
// Commands cross to the storage owner. Tokens prevent old completions after edits/reboots.
class GatewayIO {
public:
  virtual ~GatewayIO() = default;
  virtual bool reserve(uint8_t index, uint16_t net, uint16_t node, uint64_t session,
                       uint32_t token) = 0;
  virtual bool archive(uint8_t index, uint16_t net, uint16_t node, uint64_t session,
                       uint32_t correlation, const uint8_t bytes[446], uint64_t received) = 0;
};
class Gateway {
  GatewayIO &io;
  uint32_t token = 0;
  int archiveOwner = -1;
  Reject reject(Reject r);
  bool ack(uint8_t i, const Frame &f, uint64_t now);
  bool request(uint8_t i, uint64_t session, uint8_t mode, uint32_t day, bool challenge,
               uint64_t now);

public:
  Config config{};
  NodeState nodes[16]{};
  Counters counters{};
  Scheduler scheduler{};
  RefreshPolicy refreshPolicy{};
  uint32_t automaticPolls = 0, automaticPollErrors = 0;
  Assembly assembly{};
  explicit Gateway(GatewayIO &port) : io(port) {}
  void configure(const Config &c);
  Reject receive(const uint8_t *p, size_t n, uint64_t now, float rssi = 0, float snr = 0);
  bool poll(uint8_t i, uint8_t mode, uint32_t day, uint64_t now);
  void sequenceReady(uint8_t i, uint32_t requestToken, bool ok, uint32_t sequence, uint64_t now);
  bool nextTx(uint64_t now, TxJob &job);
  void transmitted(const TxJob &job, bool ok, uint64_t now, uint32_t actualAirMs);
  void durable(uint8_t i, uint64_t session, uint32_t corr, bool ok);
  void tick(uint64_t now);
};
} // namespace gw
