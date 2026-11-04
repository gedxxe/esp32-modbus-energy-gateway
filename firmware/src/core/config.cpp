// SPDX-License-Identifier: GPL-3.0-only
#include "config.h"
namespace gw {
bool validPhy(const Phy &p) {
  return p.hz >= 803000000 && p.hz <= 930000000 && (p.bw == 125 || p.bw == 250 || p.bw == 500) &&
         p.preamble >= 8 && p.preamble <= 64 && p.sf >= 7 && p.sf <= 12 && p.cr >= 5 && p.cr <= 8 &&
         p.dbm >= 0 && p.dbm <= 22;
}
bool hasKey(const Node &n) {
  for (uint8_t b : n.key)
    if (b)
      return true;
  return false;
}
int lookup(const Config &c, uint16_t net, uint16_t id) {
  for (unsigned i = 0; i < 16; i++)
    if (c.nodes[i].occupied && c.nodes[i].network == net && c.nodes[i].id == id)
      return int(i);
  return -1;
}
bool validConfig(const Config &c) {
  if (!c.network || !c.gateway || c.gateway == 65535 || !validPhy(c.phy))
    return false;
  for (unsigned i = 0; i < 16; i++) {
    const Node &n = c.nodes[i];
    if (!n.occupied)
      continue;
    if (!n.network || !n.id || n.id == 65535 || n.id == c.gateway || !n.revision ||
        !memchr(n.name, 0, sizeof(n.name)) || !memchr(n.location, 0, sizeof(n.location)) ||
        !memchr(n.notes, 0, sizeof(n.notes)) || (n.enabled && !hasKey(n)))
      return false;
    for (unsigned j = 0; j < i; j++)
      if (c.nodes[j].occupied && n.network == c.nodes[j].network && n.id == c.nodes[j].id)
        return false;
  }
  return true;
}
void encodeConfig(const Config &c, uint8_t *p) {
  memset(p, 0, ConfigBytes);
  p[0] = c.enabled;
  p16(p + 2, c.network);
  p16(p + 4, c.gateway);
  p32(p + 8, c.revision);
  p32(p + 12, c.phy.hz);
  p16(p + 16, c.phy.bw);
  p16(p + 18, c.phy.preamble);
  p[20] = c.phy.sf;
  p[21] = c.phy.cr;
  p[22] = c.phy.sync;
  p[23] = uint8_t(c.phy.dbm);
  for (unsigned i = 0; i < 16; i++) {
    const Node &n = c.nodes[i];
    uint8_t *q = p + 32 + i * 224;
    q[0] = n.occupied;
    q[1] = n.enabled;
    p16(q + 2, n.network);
    p16(q + 4, n.id);
    p32(q + 8, n.revision);
    memcpy(q + 12, n.key, 32);
    memcpy(q + 44, n.name, 33);
    memcpy(q + 77, n.location, 49);
    memcpy(q + 126, n.notes, 97);
  }
}
bool decodeConfig(const uint8_t *p, size_t n, Config &c) {
  if (n != ConfigBytes || p[0] > 1)
    return false;
  Config x;
  x.enabled = p[0];
  x.network = u16(p + 2);
  x.gateway = u16(p + 4);
  x.revision = u32(p + 8);
  x.phy.hz = u32(p + 12);
  x.phy.bw = u16(p + 16);
  x.phy.preamble = u16(p + 18);
  x.phy.sf = p[20];
  x.phy.cr = p[21];
  x.phy.sync = p[22];
  x.phy.dbm = int8_t(p[23]);
  for (unsigned i = 0; i < 16; i++) {
    const uint8_t *q = p + 32 + i * 224;
    Node &v = x.nodes[i];
    if (q[0] > 1 || q[1] > 1)
      return false;
    v.occupied = q[0];
    v.enabled = q[1];
    v.network = u16(q + 2);
    v.id = u16(q + 4);
    v.revision = u32(q + 8);
    memcpy(v.key, q + 12, 32);
    memcpy(v.name, q + 44, 33);
    memcpy(v.location, q + 77, 49);
    memcpy(v.notes, q + 126, 97);
  }
  if (!validConfig(x))
    return false;
  c = x;
  return true;
}
} // namespace gw
