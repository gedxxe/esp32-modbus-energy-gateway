// SPDX-License-Identifier: GPL-3.0-only
#include "scheduler.h"
namespace gw {
// RadioLib 7.8.1 SX126x LoRa equation (explicit header, CRC on, auto LDRO).
uint32_t airtimeUs(const Phy &p, size_t n) {
  double symbol = double(1u << p.sf) / (p.bw * 1000.);
  int de = symbol >= .016 ? 1 : 0;
  double payload =
      8 + std::fmax(0., std::ceil((8. * n - 4 * p.sf + 28 + 16) / (4. * (p.sf - 2 * de))) * p.cr);
  return uint32_t(std::ceil((p.preamble + 4.25 + payload) * symbol * 1e6));
}
bool Scheduler::enqueue(const TxJob &job) {
  if (job.frame.type == Ack)
    for (auto &x : jobs)
      if (x.used && x.frame.type == Ack && x.node == job.node &&
          x.frame.session == job.frame.session && x.frame.sequence == job.frame.sequence &&
          x.frame.payload[0] == job.frame.payload[0]) {
        coalesced++;
        return true;
      }
  for (auto &x : jobs)
    if (!x.used) {
      x = job;
      x.used = true;
      depth++;
      return true;
    }
  drops++;
  return false;
}
bool Scheduler::pop(uint64_t now, TxJob &out) {
  for (auto &x : jobs)
    if (x.used && now >= x.expires) {
      x.used = false;
      depth--;
      expired++;
    }
  if (now < nextTx)
    return false;
  for (unsigned priority = 0; priority < 2; priority++)
    for (unsigned j = 0; j < 16; j++) {
      unsigned node = (cursor + j) % 16;
      for (auto &x : jobs)
        if (x.used && x.node == node && now >= x.ready &&
            ((x.frame.type == Ack) == (priority == 0))) {
          out = x;
          x.used = false;
          depth--;
          cursor = uint8_t((node + 1) % 16);
          return true;
        }
    }
  return false;
}
void Scheduler::cancelNode(uint8_t i) {
  for (auto &x : jobs)
    if (x.used && x.node == i) {
      x.used = false;
      depth--;
      drops++;
    }
}
void Scheduler::clear() {
  for (auto &x : jobs)
    x.used = false;
  depth = 0;
}
} // namespace gw
