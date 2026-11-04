// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "bytes.h"
namespace gw {
constexpr size_t InventoryCapacity = 16;
struct Phy {
  uint32_t hz = 915000000;
  uint16_t bw = 125, preamble = 8;
  uint8_t sf = 7, cr = 5, sync = 0x12;
  int8_t dbm = 14;
};
bool validPhy(const Phy &p);
struct Node {
  bool occupied = false, enabled = false;
  uint16_t network = 1, id = 2;
  uint32_t revision = 0;
  char name[33]{}, location[49]{}, notes[97]{};
  uint8_t key[32]{};
};
struct Config {
  bool enabled = false;
  uint16_t network = 1, gateway = 1;
  uint32_t revision = 0;
  Phy phy{};
  Node nodes[16]{};
};
bool hasKey(const Node &n);
bool validConfig(const Config &c);
int lookup(const Config &c, uint16_t network, uint16_t node);
constexpr size_t ConfigBytes = 32 + 16 * 224;
void encodeConfig(const Config &c, uint8_t *p);
bool decodeConfig(const uint8_t *p, size_t n, Config &c);
} // namespace gw
