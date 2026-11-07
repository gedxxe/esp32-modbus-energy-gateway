// SPDX-License-Identifier: GPL-3.0-only
#include "gateway.h"
namespace gw {
const char *requestName(RequestState s) {
  static const char *const n[] = {
      "idle",    "reserving",           "queued", "waiting", "validated",
      "durable", "timeout_unavailable", "error"};
  return n[unsigned(s)];
}
int Replay::check(uint32_t seq, const uint8_t tag[16]) const {
  if (!high || forward(seq, high))
    return 0;
  uint32_t back = high - seq;
  if (back >= ReplayWindow)
    return -1;
  if (!(bits & (uint64_t(1) << back)))
    return 0;
  return equal(tags[seq % ReplayWindow], tag, 16) ? 1 : -1;
}
void Replay::accept(uint32_t seq, const uint8_t tag[16]) {
  if (!high) {
    high = seq;
    bits = 1;
  } else if (forward(seq, high)) {
    uint32_t d = seq - high;
    bits = d >= ReplayWindow ? 1 : (bits << d) | 1;
    high = seq;
  } else
    bits |= uint64_t(1) << (high - seq);
  memcpy(tags[seq % ReplayWindow], tag, 16);
}
Reject Gateway::reject(Reject r) {
  switch (r) {
  case Reject::Auth:
    counters.auth++;
    break;
  case Reject::Unknown:
    counters.unknown++;
    break;
  case Reject::Disabled:
    counters.disabled++;
    break;
  case Reject::Address:
    counters.address++;
    break;
  case Reject::Session:
    counters.session++;
    break;
  case Reject::Replay:
    counters.replay++;
    break;
  case Reject::Correlation:
    counters.correlation++;
    break;
  case Reject::Queue:
    counters.queueDrops++;
    break;
  case Reject::Storage:
    counters.storageErrors++;
    break;
  default:
    counters.malformed++;
    break;
  }
  return r;
}
void Gateway::configure(const Config &c) {
  scheduler.clear();
  refreshPolicy.reset();
  automaticPolls = automaticPollErrors = 0;
  cadBusy = cadClear = cadErrors = cadDeferred = 0;
  assembly = Assembly{};
  archiveOwner = -1;
  for (unsigned i = 0; i < 16; i++)
    nodes[i] = NodeState{};
  config = c;
}
void Gateway::retainDutyQueue(uint64_t now) {
  scheduler.protectDutyQueue(now, config.phy);
  for (unsigned i = 0; i < InventoryCapacity; ++i) {
    Pending &pending = nodes[i].pending;
    if (pending.state != RequestState::Queued)
      continue;
    // Only an unexpired, still-enqueued POLL may extend its request state.
    // Otherwise leave the original deadline for tick() to retire it.
    uint64_t queued = scheduler.queuedPollExpiry(uint8_t(i), pending.correlation,
                                                 pending.session, now);
    if (queued && pending.deadline < queued)
      pending.deadline = queued;
  }
}
bool Gateway::ack(uint8_t i, const Frame &f, uint64_t now) {
  TxJob j;
  j.node = i;
  j.frame = acknowledge(f);
  j.ready = now;
  // The node accepts a matching late ACK during retry/duty backoff.
  // At high spreading factors a previous gateway transmission can
  // block ACK TX for >60 s; do not discard it before the duty gate opens.
  const uint64_t minimumExpiry = now + 60000;
  const uint64_t dutyExpiry = scheduler.nextTx + 30000;
  j.expires = dutyExpiry > minimumExpiry ? dutyExpiry : minimumExpiry;
  j.revision = config.revision;
  if (!scheduler.enqueue(j))
    return false;
  retainDutyQueue(now);
  return true;
}
bool Gateway::request(uint8_t i, uint64_t sess, uint8_t mode, uint32_t day, bool challenge,
                      uint64_t now) {
  NodeState &n = nodes[i];
  Pending &p = n.pending;
  if (p.state == RequestState::Reserving || p.state == RequestState::Queued ||
      p.state == RequestState::Waiting || p.state == RequestState::Validated)
    return false;
  if (mode == 1 && archiveOwner >= 0)
    return false;
  p = Pending{};
  p.session = sess;
  p.mode = mode;
  p.day = day;
  p.challenge = challenge;
  p.token = ++token;
  if (!token)
    p.token = ++token;
  p.deadline = now + 30000;
  p.state = RequestState::Reserving;
  const Node &entry = config.nodes[i];
  if (!io.reserve(i, entry.network, entry.id, sess, p.token)) {
    p.state = RequestState::Error;
    counters.queueDrops++;
    return false;
  }
  if (mode == 1)
    archiveOwner = i;
  return true;
}
bool Gateway::poll(uint8_t i, uint8_t mode, uint32_t day, uint64_t now) {
  if (i >= 16 || mode > 1 || (day && !validDay(day)) || !config.enabled ||
      !config.nodes[i].enabled || !nodes[i].current.nonce)
    return false;
  return request(i, nodes[i].current.nonce, mode, day, false, now);
}
void Gateway::sequenceReady(uint8_t i, uint32_t t, bool ok, uint32_t seq, uint64_t now) {
  if (i >= 16)
    return;
  Pending &p = nodes[i].pending;
  if (p.state != RequestState::Reserving || p.token != t)
    return;
  if (!ok || !seq) {
    p.state = RequestState::Error;
    counters.storageErrors++;
    if (archiveOwner == i)
      archiveOwner = -1;
    return;
  }
  p.correlation = seq;
  TxJob j;
  j.node = i;
  j.frame.type = Poll;
  j.frame.length = 5;
  j.frame.network = config.nodes[i].network;
  j.frame.source = config.gateway;
  j.frame.destination = config.nodes[i].id;
  j.frame.session = p.session;
  j.frame.sequence = seq;
  j.frame.payload[0] = p.mode;
  p32(j.frame.payload + 1, p.day);
  j.ready = now + 500;
  // At SF11/SF12, the required 1% TX quiet time after HELLO ACK can
  // exceed the old fixed 120 s POLL expiry. Never retire a pending
  // session challenge before the gateway is allowed to transmit it.
  // Allow another ACK airtime budget plus bounded CAD/backoff margin.
  const uint64_t ackQuietMs =
      (uint64_t(airtimeUs(config.phy, 41) + 999) / 1000) * 100;
  const uint64_t dutyExpiry = scheduler.nextTx + ackQuietMs + 30000;
  const uint64_t minimumExpiry = now + 120000;
  j.expires = dutyExpiry > minimumExpiry ? dutyExpiry : minimumExpiry;
  j.revision = config.revision;
  p.state = RequestState::Queued;
  p.deadline = j.expires;
  if (!scheduler.enqueue(j)) {
    p.state = RequestState::Error;
    counters.queueDrops++;
    if (archiveOwner == i)
      archiveOwner = -1;
  } else
    retainDutyQueue(now);
}
Reject Gateway::receive(const uint8_t *wire, size_t size, uint64_t now, float rssi, float snr) {
  counters.rx++;
  Reject r = inspect(wire, size);
  if (r != Reject::None)
    return reject(r);
  int index = lookup(config, u16(wire + 4), u16(wire + 6));
  if (index < 0)
    return reject(Reject::Unknown);
  const Node &entry = config.nodes[index];
  if (!entry.enabled)
    return reject(Reject::Disabled);
  if (u16(wire + 8) != config.gateway)
    return reject(Reject::Address);
  Frame f;
  r = decode(wire, size, entry.key, f);
  if (r != Reject::None)
    return reject(r);
  NodeState &n = nodes[index];
  if (f.type == Poll)
    return reject(Reject::Payload);
  if (f.type == Ack) {
    counters.lateAck++;
    return reject(Reject::Correlation);
  } // v1 node replies with data, not ACK to POLL.
  Session *session = nullptr;
  if (f.type == Hello) {
    if (n.current.nonce == f.session)
      session = &n.current;
    else if (n.candidate.nonce == f.session)
      session = &n.candidate;
    else {
      if (n.candidate.nonce || (n.lastChallenge && now - n.lastChallenge < 120000))
        return reject(Reject::Session);
      n.candidate = Session{};
      n.candidate.nonce = f.session;
      n.helloAcked = false;
      n.lastChallenge = now ? now : 1;
      session = &n.candidate;
    }
  } else if (n.current.nonce == f.session)
    session = &n.current;
  else if (n.candidate.nonce == f.session)
    session = &n.candidate;
  else
    return reject(Reject::Session);
  const uint8_t *tag = wire + size - 16;
  int replay = session->replay.check(f.sequence, tag);
  if (replay < 0)
    return reject(Reject::Replay);
  if (replay == 1) {
    if (!ack(index, f, now))
      return reject(Reject::Queue);
    counters.duplicates++;
    n.duplicates++;
    n.lastPacketReceived = now;
    n.lastSequence = f.sequence;
    n.lastType = f.type;
    n.rssi = rssi;
    n.snr = snr;
    return Reject::None;
  }
  if (f.type == Hello) {
    if (!ack(index, f, now))
      return reject(Reject::Queue);
    memcpy(n.firmware, f.payload + 2, 22);
  } else if (f.type == Telemetry) {
    Live l;
    if (!decodeLive(f, l))
      return reject(Reject::Payload);
    Pending &p = n.pending;
    bool matched = p.state == RequestState::Waiting && p.mode == 0 && p.session == f.session &&
                   p.correlation == l.correlation && now <= p.deadline;
    if (session == &n.candidate && (!matched || !p.challenge))
      return reject(Reject::Correlation);
    if (l.correlation && !matched)
      return reject(Reject::Correlation);
    if (!ack(index, f, now))
      return reject(Reject::Queue);
    if (session == &n.candidate) {
      n.current = n.candidate;
      n.candidate = Session{};
      session = &n.current;
      n.confirmed = now;
    }
    if (matched)
      p.state = RequestState::Idle;
    // A late out-of-order frame is ACKed, but cannot overwrite newer live state.
    if (!session->replay.high || forward(f.sequence, session->replay.high)) {
      n.live = l;
      n.hasLive = true;
      n.received = now;
      if ((l.flags & 1) && std::isfinite(l.kwh)) {
        n.lastValid = l;
        n.hasLastValid = true;
        n.lastValidReceived = now;
      }
    }
  } else if (f.type == Checkpoint) {
    Pending &p = n.pending;
    if (session != &n.current || archiveOwner != index || p.state != RequestState::Waiting ||
        p.mode != 1 || p.session != f.session || p.correlation != u32(f.payload))
      return reject(Reject::Correlation);
    Assembly staged = assembly;
    FragmentResult fr = staged.add(f.network, f.source, f.session, f.payload, f.length);
    if (fr == FragmentResult::Rejected)
      return reject(Reject::Payload);
    if (!ack(index, f, now))
      return reject(Reject::Queue);
    assembly = staged;
    if (fr == FragmentResult::InvalidRecord) {
      counters.recordErrors++;
      p.state = RequestState::Error;
      assembly.active = false;
      archiveOwner = -1;
    } else if (fr == FragmentResult::Complete) {
      counters.validated++;
      p.state = RequestState::Validated;
      if (!io.archive(index, f.network, f.source, f.session, p.correlation, assembly.bytes, now)) {
        p.state = RequestState::Error;
        counters.storageErrors++;
      }
      assembly.active = false;
      archiveOwner = -1;
    }
  }
  session->replay.accept(f.sequence, tag);
  n.rssi = rssi;
  n.snr = snr;
  n.lastPacketReceived = now;
  n.lastSequence = f.sequence;
  n.lastType = f.type;
  n.accepted++;
  counters.accepted++;
  return Reject::None;
}
bool Gateway::nextTx(uint64_t now, TxJob &job) {
  if (!config.enabled)
    return false;
  while (scheduler.pop(now, job)) {
    if (job.node >= InventoryCapacity || job.revision != config.revision ||
        !config.nodes[job.node].enabled)
      continue;
    if (job.frame.type == Poll) {
      const Pending &p = nodes[job.node].pending;
      if (p.state != RequestState::Queued || p.correlation != job.frame.sequence ||
          p.session != job.frame.session)
        continue; // Ignore canceled or timed-out downlinks, never send stale POLLs.
    }
    return true;
  }
  return false;
}
bool Gateway::deferChannelBusy(const TxJob &job, uint64_t now, uint32_t entropy) {
  if (job.frame.type != Poll || job.node >= InventoryCapacity)
    return false;
  TxJob retry = job;
  ++retry.cadDeferrals;
  retry.ready = now + channelBackoffMs(entropy, retry.cadDeferrals);
  Pending &p = nodes[job.node].pending;
  if (p.state != RequestState::Queued || p.correlation != job.frame.sequence ||
      p.session != job.frame.session)
    return false; // A retired request stays retired.
  if (retry.ready >= retry.expires) {
    p.state = RequestState::Unavailable;
    ++counters.timeouts;
    return false;
  }
  if (!scheduler.enqueue(retry)) {
    p.state = RequestState::Error;
    ++counters.queueDrops;
    return false;
  }
  ++cadDeferred;
  return true;
}
void Gateway::transmitted(const TxJob &j, bool ok, uint64_t now, uint32_t airMs) {
  scheduler.charge(now, airMs);
  retainDutyQueue(now);
  if (j.revision != config.revision)
    return;
  NodeState &n = nodes[j.node];
  if (!ok) {
    counters.timeouts++;
    if (j.frame.type == Poll) {
      n.pending.state = RequestState::Error;
      if (archiveOwner == j.node) {
        archiveOwner = -1;
        assembly.active = false;
      }
    }
    return;
  }
  counters.tx++;
  if (j.frame.type == Ack) {
    counters.acks++;
    if (j.frame.payload[0] == Hello && n.candidate.nonce == j.frame.session) {
      n.helloAcked = true;
      if (n.pending.state != RequestState::Reserving && n.pending.state != RequestState::Queued &&
          n.pending.state != RequestState::Waiting && n.pending.state != RequestState::Validated)
        request(j.node, j.frame.session, 0, 0, true, now);
    }
  } else {
    Pending &p = n.pending;
    if (p.state == RequestState::Queued && p.correlation == j.frame.sequence &&
        p.session == j.frame.session) {
      p.state = RequestState::Waiting; // covers max node contention + 1% gate + retry/owner waits
                                       // at selected PHY.
      uint64_t quiet = (uint64_t(airtimeUs(config.phy, 152) + 999) / 1000) * 100;
      p.deadline = now + 30000 + quiet * 4 + 60000;
      if (p.mode == 1) {
        p.deadline += quiet * 8;
        assembly.start(config.nodes[j.node].network, config.nodes[j.node].id, p.session,
                       p.correlation, p.day);
      }
    }
  }
}
void Gateway::durable(uint8_t i, uint64_t sess, uint32_t corr, bool ok) {
  if (i >= 16)
    return;
  Pending &p = nodes[i].pending;
  if (p.state == RequestState::Validated && p.session == sess && p.correlation == corr) {
    p.state = ok ? RequestState::Durable : RequestState::Error;
    if (ok)
      counters.durable++;
    else
      counters.storageErrors++;
  }
}
void Gateway::tick(uint64_t now) {
  for (unsigned i = 0; i < 16; i++) {
    NodeState &n = nodes[i];
    Pending &p = n.pending;
    if ((p.state == RequestState::Reserving || p.state == RequestState::Queued ||
         p.state == RequestState::Waiting) &&
        now > p.deadline) {
      p.state = RequestState::Unavailable;
      counters.timeouts++;
      if (archiveOwner == int(i)) {
        archiveOwner = -1;
        assembly.active = false;
      }
      if (p.challenge) {
        n.candidate = Session{};
        n.helloAcked = false;
      }
    }
    if (n.candidate.nonce && p.state != RequestState::Reserving &&
        p.state != RequestState::Queued && p.state != RequestState::Waiting &&
        p.state != RequestState::Validated) {
      if (now - n.lastChallenge > 120000) {
        n.candidate = Session{};
        n.helloAcked = false;
      } else if (n.helloAcked)
        request(i, n.candidate.nonce, 0, 0, true, now);
    }
  }
  // Scheduled POLLs are deliberately lower priority than radio ACKs,
  // checkpoint transfers, a manual request and a session challenge.
  // One unresolved POLL at a time avoids a wall of pending requests
  // across a half-duplex radio shared by all nodes.
  if (!config.enabled || !refreshPolicy.available(now) ||
      scheduler.depth || now < scheduler.nextTx || archiveOwner >= 0)
    return;
  for (const auto &n : nodes)
    if (n.pending.state == RequestState::Reserving ||
        n.pending.state == RequestState::Queued ||
        n.pending.state == RequestState::Waiting ||
        n.pending.state == RequestState::Validated)
      return;
  for (unsigned offset = 0; offset < InventoryCapacity; ++offset) {
    uint8_t i = refreshPolicy.at(offset);
    const NodeState &n = nodes[i];
    const bool hasEnergy = (n.live.flags & 1) != 0;
    if (!config.nodes[i].occupied || !config.nodes[i].enabled ||
        !n.current.nonce || n.candidate.nonce || !n.hasLive ||
        !refreshPolicy.due(i, now, n.received, hasEnergy))
      continue;
    const bool ok = request(i, n.current.nonce, 0, 0, false, now);
    refreshPolicy.attempted(i, now);
    if (ok)
      ++automaticPolls;
    else
      ++automaticPollErrors;
    return;
  }
}
} // namespace gw
