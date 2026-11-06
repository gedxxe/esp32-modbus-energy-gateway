// SPDX-License-Identifier: GPL-3.0-only
// Cross-compile the two real production wire codecs on the same host.
// The node header is read directly from a pinned checkout, not copied here.
#include "core/protocol.h"
#include "lora_protocol.h"
#include <cstring>
#include <cmath>
#include <iostream>
#include <cstdlib>
#define REQUIRE(x) do { if (!(x)) { std::cerr<<"FAIL PTMP interop line "<<__LINE__<<": "<<#x<<"\n"; return 1; } } while(0)

int main() {
  unsigned passes = 0;
  for (uint16_t nodeId = 2; nodeId <= 17; ++nodeId) {
    uint8_t key[32] = {};
    for (unsigned i = 0; i < 32; ++i) key[i] = uint8_t(29 + i + nodeId * 7);
    fort::lora::Settings settings;
    settings.enabled = true;
    settings.nodeId = nodeId;
    settings.gatewayId = 1;
    settings.networkId = 1;
    std::memcpy(settings.key, key, sizeof(key));
    REQUIRE(fort::lora::valid(settings));
    REQUIRE(settings.frequencyHz == 915000000 && settings.powerDbm == 14);
    fort::lora::Frame nodeHello;
    nodeHello.type = fort::lora::HELLO;
    nodeHello.length = 24;
    nodeHello.network = settings.networkId;
    nodeHello.source = settings.nodeId;
    nodeHello.destination = settings.gatewayId;
    nodeHello.sequence = 1;
    nodeHello.session = 0x1020304050607000ULL + nodeId;
    nodeHello.payload[0] = 3;
    nodeHello.payload[1] = 7;
    std::memcpy(nodeHello.payload + 2, "3.5.0", 5);
    uint8_t wire[fort::lora::MAX_PACKET] = {};
    const size_t encoded = fort::lora::encodeFrame(nodeHello, wire);
    REQUIRE(encoded == 48);
    uint8_t mac[32];
    gw::hmac(key, wire, encoded, mac);
    std::memcpy(wire + encoded, mac, 16);
    gw::Frame hello;
    REQUIRE(gw::decode(wire, encoded + 16, key, hello) == gw::Reject::None);
    REQUIRE(hello.source == nodeId && hello.type == gw::Hello);
    REQUIRE(hello.session == nodeHello.session);
    ++passes;

    // Gateway ACK carries the identical node session/sequence/type.
    const gw::Frame ack = gw::acknowledge(hello);
    uint8_t reply[gw::MaxPacket] = {};
    const size_t replyLen = gw::encode(ack, key, reply);
    REQUIRE(replyLen == 41);
    fort::lora::Frame parsed;
    REQUIRE(fort::lora::decodeFrame(reply, replyLen - 16, parsed));
    REQUIRE(fort::lora::addressed(parsed, settings, nodeHello.session));
    REQUIRE(fort::lora::acknowledges(parsed, nodeHello));
    ++passes;

    // Gateway's POLL is authenticatable with the same node key and address.
    gw::Frame poll;
    poll.type = gw::Poll;
    poll.length = 5;
    poll.network = 1;
    poll.source = 1;
    poll.destination = nodeId;
    poll.sequence = 0x12u + nodeId;
    poll.session = nodeHello.session;
    poll.payload[0] = 0; // latest cached telemetry
    const size_t pollLen = gw::encode(poll, key, reply);
    REQUIRE(pollLen == 45);
    REQUIRE(fort::lora::decodeFrame(reply, pollLen - 16, parsed));
    REQUIRE(fort::lora::addressed(parsed, settings, nodeHello.session));
    REQUIRE(parsed.type == fort::lora::POLL && parsed.payload[0] == 0);
    ++passes;

    // Node telemetry is already scaled to engineering kWh, gateway must
    // preserve it with the request correlation, not apply PT/CT again.
    fort::lora::Frame tele = nodeHello;
    tele.type = fort::lora::TELEMETRY;
    tele.length = 32;
    std::memset(tele.payload, 0, sizeof(tele.payload));
    tele.sequence = 3;
    fort::lora::put32(tele.payload, poll.sequence);
    fort::lora::put32(tele.payload + 8, 300);
    tele.payload[12] = 1;
    tele.payload[13] = 5; // verified Acrel ADL400 profile
    const double engineering = 7838.40;
    uint64_t bits;
    std::memcpy(&bits, &engineering, sizeof(bits));
    fort::lora::put64(tele.payload + 24, bits);
    const size_t teleBody = fort::lora::encodeFrame(tele, wire);
    REQUIRE(teleBody == 56);
    gw::hmac(key, wire, teleBody, mac);
    std::memcpy(wire + teleBody, mac, 16);
    gw::Frame decodedTele;
    REQUIRE(gw::decode(wire, teleBody + 16, key, decodedTele) == gw::Reject::None);
    gw::Live sample;
    REQUIRE(gw::decodeLive(decodedTele, sample));
    REQUIRE(sample.correlation == poll.sequence && sample.profile == 5);
    REQUIRE(std::fabs(sample.kwh - 7838.40) < 0.000001);
    ++passes;

    // Authenticated packets cannot be relabelled to a different node key.
    wire[7] ^= 1;
    REQUIRE(gw::decode(wire, teleBody + 16, key, decodedTele) != gw::Reject::None);
    ++passes;

    // Client CHECKPOINT fragment must use the same signed codec and gateway
    // framing. The maximum fragment is 12 metadata + 100 data bytes.
    fort::lora::Frame fragment = nodeHello;
    fragment.type = fort::lora::CHECKPOINT;
    fragment.length = 112;
    fragment.sequence = 4;
    std::memset(fragment.payload, 0, sizeof(fragment.payload));
    fort::lora::put32(fragment.payload, poll.sequence);
    fort::lora::put32(fragment.payload + 4, 20000);
    fort::lora::put16(fragment.payload + 8, 0);
    fort::lora::put16(fragment.payload + 10, 446);
    for (unsigned b = 0; b < 100; ++b)
      fragment.payload[12 + b] = uint8_t(b + 1);
    const size_t fragmentBody = fort::lora::encodeFrame(fragment, wire);
    REQUIRE(fragmentBody == 136);
    gw::hmac(key, wire, fragmentBody, mac);
    std::memcpy(wire + fragmentBody, mac, 16);
    gw::Frame decodedFragment;
    REQUIRE(gw::decode(wire, fragmentBody + 16, key, decodedFragment) == gw::Reject::None);
    REQUIRE(decodedFragment.type == gw::Checkpoint &&
            gw::u32(decodedFragment.payload) == poll.sequence);
    REQUIRE(gw::u16(decodedFragment.payload + 8) == 0 &&
            gw::u16(decodedFragment.payload + 10) == 446);
    const gw::Frame archiveAck = gw::acknowledge(decodedFragment);
    const size_t archiveAckLen = gw::encode(archiveAck, key, reply);
    REQUIRE(archiveAckLen == 41);
    REQUIRE(fort::lora::decodeFrame(reply, archiveAckLen - 16, parsed));
    REQUIRE(fort::lora::addressed(parsed, settings, fragment.session));
    REQUIRE(fort::lora::acknowledges(parsed, fragment));
    ++passes;

  }
  std::cout << "PASS: " << passes
            << " cross-firmware wire checks (HELLO/ACK/POLL/TELEMETRY/CHECKPOINT, 16 node IDs)\n";
}
