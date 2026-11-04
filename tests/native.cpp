// SPDX-License-Identifier: GPL-3.0-only
#include "core/gateway.h"
#include "core/radio_lifecycle.h"
#include "storage/sequences.h"
#include "storage/archive.h"
#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <cstdlib>
#include <algorithm>
using namespace gw;
unsigned checks = 0;
#define CHECK(x)                                                                                   \
  do {                                                                                             \
    checks++;                                                                                      \
    if (!(x)) {                                                                                    \
      std::cerr << "FAIL line " << __LINE__ << ": " << #x << "\n";                                 \
      std::exit(1);                                                                                \
    }                                                                                              \
  } while (0)
struct Memory : Backend {
  std::map<std::string, std::vector<uint8_t>> data;
  bool partial = false, fail = false, readFail = false;
  size_t size(const char *k) override {
    return data[k].size();
  }
  size_t read(const char *k, uint8_t *p, size_t n) override {
    if (readFail)
      return 0;
    auto &v = data[k];
    size_t z = std::min(n, v.size());
    std::copy_n(v.begin(), z, p);
    return z;
  }
  size_t write(const char *k, const uint8_t *p, size_t n) override {
    if (fail)
      return 0;
    size_t z = partial ? n / 2 : n;
    data[k] = std::vector<uint8_t>(p, p + z);
    return z;
  }
};
struct IO : GatewayIO {
  struct R {
    uint8_t index;
    uint16_t network, node;
    uint64_t session;
    uint32_t token;
  };
  std::vector<R> reservations;
  bool full = false;
  unsigned saved = 0;
  uint8_t record[446]{};
  bool reserve(uint8_t i, uint16_t net, uint16_t node, uint64_t sess, uint32_t token) override {
    if (full)
      return false;
    reservations.push_back({i, net, node, sess, token});
    return true;
  }
  bool archive(uint8_t, uint16_t, uint16_t, uint64_t, uint32_t, const uint8_t *p,
               uint64_t) override {
    if (full)
      return false;
    memcpy(record, p, 446);
    saved++;
    return true;
  }
};
Config config() {
  Config c;
  c.enabled = true;
  c.revision = 1;
  for (unsigned i = 0; i < 3; i++) {
    Node &n = c.nodes[i];
    n.occupied = n.enabled = true;
    n.network = 1;
    n.id = uint16_t(i + 2);
    n.revision = 1;
    strcpy(n.name, "Test node");
    for (unsigned j = 0; j < 32; j++)
      n.key[j] = uint8_t(61 + i * 37 + j);
  }
  return c;
}
Frame frame(const Config &c, unsigned i, uint8_t type, uint32_t seq, uint64_t sess) {
  Frame f;
  f.type = type;
  f.network = c.nodes[i].network;
  f.source = c.nodes[i].id;
  f.destination = c.gateway;
  f.sequence = seq;
  f.session = sess;
  if (type == Hello) {
    f.length = 24;
    f.payload[0] = 3;
    f.payload[1] = 7;
    memcpy(f.payload + 2, "3.4.3", 5);
  } else if (type == Telemetry) {
    f.length = 32;
    f.payload[12] = 9;
    f.payload[13] = 5;
    p32(f.payload + 8, 300);
    double kwh = 123.456;
    uint64_t b;
    memcpy(&b, &kwh, 8);
    p64(f.payload + 24, b);
  }
  return f;
}
std::vector<uint8_t> wire(const Frame &f, const uint8_t *key) {
  uint8_t b[152];
  size_t n = encode(f, key, b);
  CHECK(n > 0);
  return {b, b + n};
}
Reject receive(Gateway &g, const Frame &f, unsigned i, uint64_t now) {
  auto p = wire(f, g.config.nodes[i].key);
  return g.receive(p.data(), p.size(), now, -75, 9);
}
uint64_t establish(Gateway &g, IO &io, unsigned i, uint64_t sess, uint64_t now,
                   uint32_t pollSeq = 1) {
  auto h = frame(g.config, i, Hello, 1, sess);
  CHECK(receive(g, h, i, now) == Reject::None);
  CHECK(!g.nodes[i].current.nonce);
  CHECK(g.nodes[i].candidate.nonce == sess);
  TxJob j;
  now = std::max(now, g.scheduler.nextTx);
  CHECK(g.nextTx(now, j));
  CHECK(j.frame.type == Ack && j.frame.payload[0] == Hello && j.frame.sequence == 1 &&
        j.frame.session == sess && j.frame.destination == g.config.nodes[i].id);
  g.transmitted(j, true, now, 88);
  CHECK(!io.reservations.empty());
  auto r = io.reservations.back();
  g.sequenceReady(i, r.token, true, pollSeq, now);
  now = g.scheduler.nextTx + 500;
  CHECK(g.nextTx(now, j));
  CHECK(j.frame.type == Poll && j.frame.session == sess && j.frame.sequence == pollSeq);
  g.transmitted(j, true, now, 93);
  auto live = frame(g.config, i, Telemetry, 2, sess);
  p32(live.payload, pollSeq);
  CHECK(receive(g, live, i, now + 300) == Reject::None);
  CHECK(g.nodes[i].current.nonce == sess && g.nodes[i].hasLive);
  now = g.scheduler.nextTx + 500;
  CHECK(g.nextTx(now, j));
  g.transmitted(j, true, now, 88);
  return now + 1000;
}
void makeRecord(uint8_t *b, uint32_t day = 20000) {
  memset(b, 0, 446);
  p32(b, 0x31545043);
  p16(b + 4, 3);
  p16(b + 6, 10);
  p32(b + 8, day);
  p32(b + 12, 1);
  p32(b + 16, 1);
  p32(b + 20, 0x06001900);
  for (unsigned i = 0; i < 10; i++)
    b[24 + i * 42] = 2;
  uint8_t *p = b + 24;
  p[0] = 1;
  p[1] = 1;
  p[2] = 'N';
  p32(p + 4, day * 86400 + 360 * 60 - 25200);
  p32(p + 8, u32(p + 4) + 1);
  p32(p + 12, 12345);
  p32(p + 16, 9600);
  p16(p + 20, 2);
  p16(p + 22, 1);
  p16(p + 24, 3);
  double value = double(12345) * .001 * 2 * 3;
  uint64_t bits;
  memcpy(&bits, &value, 8);
  p64(p + 26, bits);
  p32(p + 34, 1);
  p16(p + 40, 17);
  p16(b + 444, crc16(b, 444));
}
Frame fragment(const Config &c, unsigned i, uint32_t seq, uint64_t sess, uint32_t corr,
               const uint8_t *record, unsigned off) {
  Frame f = frame(c, i, Checkpoint, seq, sess);
  size_t z = std::min(size_t(100), size_t(446 - off));
  f.length = uint8_t(12 + z);
  p32(f.payload, corr);
  p32(f.payload + 4, 20000);
  p16(f.payload + 8, uint16_t(off));
  p16(f.payload + 10, 446);
  memcpy(f.payload + 12, record + off, z);
  return f;
}
void codec() {
  Config blank;
  CHECK(!blank.enabled);
  for (const Node &node : blank.nodes)
    CHECK(!node.occupied && !hasKey(node));
  uint8_t hash[32];
  sha256(reinterpret_cast<const uint8_t *>("abc"), 3, hash);
  const uint8_t expected[32] = {0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40,
                                0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17,
                                0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
  CHECK(equal(hash, expected, 32));
  Config c = config();
  CHECK(validConfig(c));
  Frame f = frame(c, 0, Hello, 7, 0x0807060504030201ULL);
  auto p = wire(f, c.nodes[0].key);
  Frame decoded;
  CHECK(decode(p.data(), p.size(), c.nodes[0].key, decoded) == Reject::None);
  CHECK(decoded.sequence == 7 && decoded.session == f.session);
  for (size_t n = 0; n < p.size(); n++)
    CHECK(inspect(p.data(), n) != Reject::None);
  auto wrong = p;
  wrong.back() ^= 1;
  CHECK(decode(wrong.data(), wrong.size(), c.nodes[0].key, decoded) == Reject::Auth);
  for (size_t i = 0; i < p.size(); i++) {
    auto q = p;
    q[i] ^= 0x80;
    CHECK(decode(q.data(), q.size(), c.nodes[0].key, decoded) != Reject::None);
  }
  p[11] = 1;
  CHECK(inspect(p.data(), p.size()) == Reject::Header);
  f = frame(c, 0, Telemetry, 2, 123);
  f.payload[12] = 8;
  memset(f.payload + 24, 0, 8);
  Live l;
  CHECK(decodeLive(f, l) && std::isnan(l.kwh));
  f.payload[12] = 9;
  p32(f.payload + 8, 15001);
  CHECK(!validPayload(f));
  c.nodes[1].id = 2;
  CHECK(!validConfig(c));
  c = config();
  c.nodes[1].network = 2;
  CHECK(validConfig(c));
  c.nodes[0].id = 1;
  CHECK(!validConfig(c));
}
void storage() {
  Memory mem;
  Banks b(mem, "a", "b", 1);
  uint8_t p[4] = {1, 2, 3, 4}, o[4];
  CHECK(b.load(o, 4) == Load::Empty);
  CHECK(b.save(p, 4));
  p[0] = 2;
  mem.partial = true;
  CHECK(!b.save(p, 4));
  Banks recovery(mem, "a", "b", 1);
  CHECK(recovery.load(o, 4) == Load::Ok && o[0] == 1);
  Banks strict(mem, "a", "b", 1);
  CHECK(strict.load(o, 4, true) == Load::Corrupt);
  mem.partial = false;
  CHECK(recovery.save(p, 4));
  Banks again(mem, "a", "b", 1);
  CHECK(again.load(o, 4) == Load::Ok && o[0] == 2);
  Memory m;
  Banks sb(m, "sa", "sb", 2);
  Sequences seq(sb);
  CHECK(seq.load(true));
  uint32_t s;
  CHECK(seq.take(1, 2, 44, s) && s == 1);
  CHECK(seq.take(1, 2, 44, s) && s == 2);
  Banks rb(m, "sa", "sb", 2);
  Sequences reboot(rb);
  CHECK(reboot.load(false));
  CHECK(reboot.take(1, 2, 44, s) && s == 65);
  m.partial = true;
  for (unsigned i = 0; i < 63; i++)
    CHECK(reboot.take(1, 2, 44, s));
  CHECK(!reboot.take(1, 2, 44, s));
  Banks cb(m, "sa", "sb", 2);
  Sequences corrupt(cb);
  CHECK(!corrupt.load(false));
  Memory lost;
  Banks lb(lost, "sa", "sb", 2);
  Sequences ls(lb);
  CHECK(!ls.load(false));
  Memory full;
  Banks fb(full, "sa", "sb", 2);
  Sequences fs(fb);
  CHECK(fs.load(true));
  for (unsigned i = 0; i < 64; i++)
    CHECK(fs.take(1, 2, 100 + i, s));
  CHECK(!fs.take(1, 2, 999, s));
  Config c = config(), d;
  uint8_t cfg[ConfigBytes];
  encodeConfig(c, cfg);
  CHECK(decodeConfig(cfg, sizeof(cfg), d));
  CHECK(d.nodes[2].id == 4 && equal(d.nodes[2].key, c.nodes[2].key, 32));
  cfg[32 + 44] = 0xff; // UTF-8 not interpreted by codec; termination stays bounded.
  CHECK(decodeConfig(cfg, sizeof(cfg), d));
  Memory inventoryFlash;
  Banks inventoryBanks(inventoryFlash, "cfg_a", "cfg_b", 1);
  encodeConfig(c, cfg);
  CHECK(inventoryBanks.save(cfg, sizeof(cfg)));
  c.nodes[3] = c.nodes[0];
  c.nodes[3].id = 5;
  strcpy(c.nodes[3].name, "Panel D");
  strcpy(c.nodes[3].location, "Workshop");
  strcpy(c.nodes[3].notes, "Commissioned through local AP");
  c.nodes[3].key[0] ^= 0x55;
  c.revision++;
  encodeConfig(c, cfg);
  CHECK(inventoryBanks.save(cfg, sizeof(cfg)));
  Banks inventoryReboot(inventoryFlash, "cfg_a", "cfg_b", 1);
  CHECK(inventoryReboot.load(cfg, sizeof(cfg)) == Load::Ok);
  CHECK(decodeConfig(cfg, sizeof(cfg), d));
  CHECK(d.nodes[3].id == 5 && d.nodes[3].enabled && !strcmp(d.nodes[3].name, "Panel D"));
  CHECK(!strcmp(d.nodes[3].location, "Workshop") && equal(d.nodes[3].key, c.nodes[3].key, 32));
  inventoryFlash.partial = true;
  c.nodes[3].id = 6;
  encodeConfig(c, cfg);
  CHECK(!inventoryReboot.save(cfg, sizeof(cfg)));
  Banks inventoryRecovery(inventoryFlash, "cfg_a", "cfg_b", 1);
  CHECK(inventoryRecovery.load(cfg, sizeof(cfg)) == Load::Ok);
  CHECK(decodeConfig(cfg, sizeof(cfg), d) && d.nodes[3].id == 5);
}
void reassembly() {
  uint8_t b[446];
  makeRecord(b);
  Record r;
  CHECK(decodeRecord(b, 446, r));
  CHECK(r.points[0].meterId == 17 && r.points[0].profile == 0 && r.points[1].status == 2);
  auto c = config();
  Assembly a;
  a.start(1, 2, 100, 9, 20000);
  for (unsigned off : {200u, 0u, 400u, 100u}) {
    Frame f = fragment(c, 0, off + 1, 100, 9, b, off);
    CHECK(a.add(1, 2, 100, f.payload, f.length) == FragmentResult::Partial);
  }
  Frame f = fragment(c, 0, 4, 100, 9, b, 300);
  CHECK(a.add(1, 2, 100, f.payload, f.length) == FragmentResult::Complete);
  CHECK(a.add(1, 2, 100, f.payload, f.length) == FragmentResult::Duplicate);
  f.payload[12] ^= 1;
  CHECK(a.add(1, 2, 100, f.payload, f.length) == FragmentResult::Rejected);
  CHECK(a.add(1, 3, 100, f.payload, f.length) == FragmentResult::Rejected);
  CHECK(a.add(1, 2, 101, f.payload, f.length) == FragmentResult::Rejected);
  Assembly missing;
  missing.start(1, 2, 100, 9, 0);
  f = fragment(c, 0, 1, 100, 9, b, 0);
  CHECK(missing.add(1, 2, 100, f.payload, f.length) == FragmentResult::Partial);
  f = fragment(c, 0, 2, 100, 9, b, 50);
  CHECK(missing.add(1, 2, 100, f.payload, f.length) == FragmentResult::Rejected);
  CHECK(!missing.complete);
  b[100] ^= 1;
  CHECK(!decodeRecord(b, 446, r));
  p16(b + 444, crc16(b, 444));
  CHECK(!decodeRecord(b, 446, r)); // corruption hits non-recorded reserved data.
  Memory m;
  Archives archives(m);
  CHECK(archives.load());
  makeRecord(b);
  CHECK(archives.save(1, 2, 100, 9, b, 123, 999));
  CHECK(archives.save(1, 2, 100, 9, b, 124, 1000));
  CHECK(archives.count == 1);
  CHECK(archives.save(1, 3, 200, 10, b, 124, 1000));
  CHECK(archives.count == 2);
  uint8_t raw[ArchiveBytes];
  CHECK(archives.read(0, raw) && u16(raw + 2) == 2);
  Archives reboot(m);
  CHECK(reboot.load() && reboot.count == 2);
  m.partial = true;
  p32(b + 12, 2);
  p16(b + 444, crc16(b, 444));
  CHECK(!archives.save(1, 2, 100, 11, b, 123, 999));
  Archives interrupted(m);
  CHECK(!interrupted.load());
  CHECK(interrupted.count == 2);
}
void sessions() {
  IO io;
  Gateway g(io);
  g.configure(config());
  uint64_t now = 1000;
  for (unsigned i = 0; i < 3; i++)
    now = establish(g, io, i, 100 + i, now);
  CHECK(g.nodes[0].live.kwh == 123.456 && g.nodes[2].current.nonce == 102);
  auto f = frame(g.config, 0, Telemetry, 3, 100);
  CHECK(receive(g, f, 0, now) == Reject::None);
  CHECK(receive(g, f, 0, now + 1) == Reject::None);
  CHECK(g.counters.duplicates == 1);
  CHECK(g.scheduler.coalesced >= 1);
  CHECK(g.nodes[0].lastPacketReceived == now + 1);
  CHECK(g.nodes[0].received == now); // Duplicate RX proves observation, not fresh measurement.
  CHECK(g.nodes[0].lastType == Telemetry && g.nodes[0].lastSequence == 3);
  CHECK(g.nodes[0].duplicates == 1 && g.nodes[1].duplicates == 0);
  double saved = g.nodes[0].live.kwh;
  f.payload[24] ^= 1;
  CHECK(receive(g, f, 0, now + 2) == Reject::Replay);
  CHECK(g.nodes[0].live.kwh == saved);
  auto wrong = frame(g.config, 1, Telemetry, 3, 100);
  CHECK(receive(g, wrong, 1, now) == Reject::Session);
  wrong.source = 88;
  CHECK(receive(g, wrong, 1, now) == Reject::Unknown);
  auto p = wire(frame(g.config, 1, Telemetry, 3, 101), g.config.nodes[0].key);
  uint64_t heard = g.nodes[1].lastPacketReceived;
  CHECK(g.receive(p.data(), p.size(), now) == Reject::Auth);
  CHECK(g.nodes[1].lastPacketReceived == heard);
  g.config.nodes[2].enabled = false;
  CHECK(receive(g, frame(g.config, 2, Telemetry, 3, 102), 2, now) == Reject::Disabled);
  g.config.nodes[2].enabled = true;
  auto ackf = frame(g.config, 0, Ack, 3, 100);
  ackf.length = 1;
  ackf.payload[0] = Hello;
  CHECK(receive(g, ackf, 0, now) == Reject::Correlation);
  CHECK(receive(g, ackf, 0, now + 100000) == Reject::Correlation);
  auto replayHello = frame(g.config, 0, Hello, 1, 100);
  size_t count = io.reservations.size();
  CHECK(receive(g, replayHello, 0, now) == Reject::None);
  CHECK(g.nodes[0].current.nonce == 100 && io.reservations.size() == count);
  auto old = frame(g.config, 0, Hello, 1, 555);
  CHECK(receive(g, old, 0, now) == Reject::Session);
  now += 120001;
  CHECK(receive(g, old, 0, now) == Reject::None);
  CHECK(g.nodes[0].current.nonce == 100 && g.nodes[0].candidate.nonce == 555);
  auto uncorrelated = frame(g.config, 0, Telemetry, 2, 555);
  CHECK(receive(g, uncorrelated, 0, now) == Reject::Correlation);
  CHECK(g.nodes[0].current.nonce == 100 && g.nodes[0].live.kwh == saved);
  auto poll = frame(g.config, 1, Poll, 9, 101);
  poll.length = 5;
  CHECK(receive(g, poll, 1, now) ==
        Reject::Payload); // replay/foreign-direction POLL never changes state.
  IO archiveIO;
  Gateway a(archiveIO);
  a.configure(config());
  now = establish(a, archiveIO, 0, 200, 1000);
  now = establish(a, archiveIO, 1, 201, now);
  CHECK(a.poll(0, 1, 20000, now));
  CHECK(!a.poll(1, 1, 20000, now));
  auto r = archiveIO.reservations.back();
  a.sequenceReady(0, r.token, true, 2, now);
  TxJob j;
  now = std::max(now, a.scheduler.nextTx) + 500;
  CHECK(a.nextTx(now, j) && j.frame.type == Poll);
  a.transmitted(j, true, now, 93);
  uint8_t b[446];
  makeRecord(b);
  for (unsigned off : {200u, 0u, 400u, 100u, 300u}) {
    auto frag = fragment(a.config, 0, 3 + off / 100, 200, 2, b, off);
    if (off == 300) {
      TxJob fill;
      fill.node = 0;
      fill.frame = acknowledge(frame(a.config, 0, Hello, 800, 200));
      fill.expires = now + 60000;
      while (a.scheduler.depth < TxCapacity) {
        fill.frame.sequence++;
        CHECK(a.scheduler.enqueue(fill));
      }
      CHECK(receive(a, frag, 0, now + 99) == Reject::Queue);
      CHECK(a.assembly.count == RecordSize - 100 && archiveIO.saved == 0);
      a.scheduler.clear();
    }
    CHECK(receive(a, frag, 0, now + 100) == Reject::None);
    auto tele = frame(a.config, 1, Telemetry, 3 + off / 100, 201);
    CHECK(receive(a, tele, 1, now + 101) == Reject::None);
    auto ownLive = frame(a.config, 0, Telemetry, 10 + off / 100, 200);
    CHECK(receive(a, ownLive, 0, now + 102) == Reject::None);
  }
  CHECK(archiveIO.saved == 1 && a.counters.validated == 1);
  CHECK(a.nodes[0].pending.state == RequestState::Validated);
  a.durable(1, 200, 2, true);
  CHECK(a.nodes[0].pending.state == RequestState::Validated);
  a.durable(0, 200, 2, true);
  CHECK(a.nodes[0].pending.state == RequestState::Durable);
  IO timeoutIO;
  Gateway timeout(timeoutIO);
  timeout.configure(config());
  now = establish(timeout, timeoutIO, 0, 300, 1000);
  CHECK(timeout.poll(0, 1, 0, now));
  r = timeoutIO.reservations.back();
  timeout.sequenceReady(0, r.token, true, 2, now);
  now = std::max(now, timeout.scheduler.nextTx) + 500;
  CHECK(timeout.nextTx(now, j));
  timeout.transmitted(j, true, now, 93);
  timeout.tick(timeout.nodes[0].pending.deadline + 1);
  CHECK(timeout.nodes[0].pending.state == RequestState::Unavailable && timeoutIO.saved == 0);
}
void retainedLastValidReading() {
  IO io;
  Gateway g(io);
  g.configure(config());
  g.nodes[0].current.nonce = 2026;
  Frame first = frame(g.config, 0, Telemetry, 1, 2026);
  CHECK(receive(g, first, 0, 1000) == Reject::None);
  CHECK(g.nodes[0].hasLastValid);
  CHECK(g.nodes[0].lastValid.kwh == 123.456);
  CHECK(g.nodes[0].lastValidReceived == 1000);
  Frame invalid = frame(g.config, 0, Telemetry, 2, 2026);
  invalid.payload[12] = 8;
  memset(invalid.payload + 24, 0, 8);
  CHECK(receive(g, invalid, 0, 1500) == Reject::None);
  CHECK(g.nodes[0].hasLive && !(g.nodes[0].live.flags & 1));
  CHECK(g.nodes[0].hasLastValid && g.nodes[0].lastValid.kwh == 123.456);
  CHECK(g.nodes[0].lastValidReceived == 1000);
}
void automaticRefresh() {
  RefreshPolicy policy;
  CHECK(!policy.due(0, 1000 + RefreshIntervalMs, 1000, true));
  CHECK(!policy.due(0, 1000 + RefreshIntervalMs + RefreshGraceMs - 1, 1000, true));
  CHECK(policy.due(0, 1000 + RefreshIntervalMs + RefreshGraceMs, 1000, true));
  CHECK(policy.due(1, 1000 + RefreshRetryMs, 1000, false));
  policy.attempted(0, 1000 + RefreshIntervalMs + RefreshGraceMs);
  CHECK(!policy.available(1000 + RefreshIntervalMs + RefreshGraceMs + RefreshSpacingMs - 1));
  CHECK(policy.available(1000 + RefreshIntervalMs + RefreshGraceMs + RefreshSpacingMs));
  CHECK(policy.at(0) == 1);
  policy.reset();
  CHECK(policy.at(0) == 0);

  IO io;
  Gateway g(io);
  g.configure(config());
  for (unsigned i = 0; i < 3; ++i) {
    g.nodes[i].current.nonce = uint64_t(1200 + i);
    g.nodes[i].hasLive = true;
    g.nodes[i].live.flags = 1;
    g.nodes[i].received = 1000;
  }
  uint64_t due = 1000 + RefreshIntervalMs + RefreshGraceMs;
  g.tick(due - 1);
  CHECK(io.reservations.empty());
  g.tick(due);
  CHECK(io.reservations.size() == 1);
  CHECK(io.reservations[0].index == 0);
  CHECK(g.automaticPolls == 1 && g.automaticPollErrors == 0);
  CHECK(g.nodes[0].pending.state == RequestState::Reserving);
  // No second request while a first one is awaiting flash reservation or RF.
  g.tick(due + RefreshSpacingMs);
  CHECK(io.reservations.size() == 1);
  g.nodes[0].pending.state = RequestState::Idle;
  // Fresh unsolicited telemetry postpones an automatic request to that node.
  g.nodes[1].received = due;
  g.tick(due + RefreshSpacingMs);
  CHECK(io.reservations.size() == 2);
  CHECK(io.reservations[1].index == 2);
  CHECK(g.automaticPolls == 2);
  // Configuration changes reset only the volatile refresh policy.
  g.configure(config());
  CHECK(g.automaticPolls == 0 && g.automaticPollErrors == 0);
}
void scheduler() {
  Scheduler s;
  auto c = config();
  TxJob p;
  p.node = 0;
  p.frame = frame(c, 0, Poll, 1, 1);
  p.frame.length = 5;
  p.ready = 0;
  p.expires = 1000;
  CHECK(s.enqueue(p));
  p.node = 1;
  p.frame = acknowledge(frame(c, 1, Hello, 1, 2));
  CHECK(s.enqueue(p));
  TxJob out;
  CHECK(s.pop(0, out) && out.frame.type == Ack);
  s.charge(0, 88);
  CHECK(!s.pop(1, out));
  CHECK(!s.pop(10000, out) && s.expired == 1);
  s.clear();
  for (unsigned i = 0; i < TxCapacity; i++) {
    p.node = i % 16;
    p.frame.sequence = i + 1;
    p.expires = 100000;
    CHECK(s.enqueue(p));
  }
  p.frame.sequence = 999;
  CHECK(!s.enqueue(p));
  CHECK(s.depth == 32);
  s.clear();
  CHECK(s.depth == 0);
  Phy phy;
  CHECK(airtimeUs(phy, 41) == 87296);
  CHECK(airtimeUs(phy, 72) == 133376);
  Scheduler fair;
  p.node = 0;
  p.frame.sequence = 70;
  CHECK(fair.enqueue(p));
  p.frame.sequence = 71;
  CHECK(fair.enqueue(p));
  p.node = 1;
  p.frame.sequence = 72;
  CHECK(fair.enqueue(p));
  CHECK(fair.pop(0, out) && out.node == 0);
  CHECK(fair.pop(0, out) && out.node == 1);
  RadioLifecycle absent;
  absent.start(false, 0);
  CHECK(absent.step(100000, true) == RadioAction::None);
  absent.start(true, 0);
  CHECK(absent.step(1, true) == RadioAction::None);
  CHECK(absent.step(2, true) == RadioAction::ReleaseReset);
  CHECK(absent.step(12, true) == RadioAction::None);
  CHECK(absent.step(111, true) == RadioAction::None);
  CHECK(absent.step(112, true) == RadioAction::BusyTimedOut);
  absent.fail(112);
  CHECK(absent.step(30111, true) == RadioAction::None);
  CHECK(absent.step(30112, true) == RadioAction::Restart);
  absent.start(true, 30112);
  CHECK(absent.step(30114, false) == RadioAction::ReleaseReset);
  CHECK(absent.step(30124, false) == RadioAction::Initialize);
  CHECK(!RadioLifecycle::txExpired(2088, 0, 88));
  CHECK(RadioLifecycle::txExpired(2089, 0, 88));
}
std::vector<uint8_t> unhex(const std::string &s) {
  std::vector<uint8_t> v;
  for (size_t i = 0; i < s.size(); i += 2)
    v.push_back(uint8_t(std::stoul(s.substr(i, 2), nullptr, 16)));
  return v;
}
int main(int argc, char **argv) {
  if (argc == 4 && std::string(argv[1]) == "--hmac") {
    auto key = unhex(argv[2]), data = unhex(argv[3]);
    if (key.size() != 32)
      return 2;
    uint8_t digest[32];
    hmac(key.data(), data.data(), data.size(), digest);
    constexpr char digits[] = "0123456789abcdef";
    for (uint8_t b : digest)
      std::cout << digits[b >> 4] << digits[b & 15];
    return 0;
  }
  if (argc == 4 && std::string(argv[1]) == "--vector") {
    auto key = unhex(argv[2]), packet = unhex(argv[3]);
    Frame f;
    CHECK(key.size() == 32);
    CHECK(decode(packet.data(), packet.size(), key.data(), f) == Reject::None);
    uint8_t out[152];
    CHECK(encode(f, key.data(), out) == packet.size());
    CHECK(equal(out, packet.data(), packet.size()));
    std::cout << "PASS production codec/HMAC public vector\n";
    return 0;
  }
  codec();
  storage();
  reassembly();
  sessions();
  scheduler();
  automaticRefresh();
  retainedLastValidReading();
  std::cout << "PASS " << checks << " production-core regression assertions\n";
}
