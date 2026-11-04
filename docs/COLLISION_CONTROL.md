# PTMP medium access, collision handling and recovery

The node and gateway form a **point-to-multipoint star network** using the same RadioLib 7.8.1 / SX1262 LoRa PHY and the Energy Station application protocol v1. Each node has its own network/node identity and 32-byte HMAC key, but **radio addressing does not allocate channel time**. Only one radio packet can be received by a single-SX1262 gateway at a time. The gateway is half-duplex and cannot receive while sending an ACK or POLL. These are radio limitations, not JSON, Modbus or PlatformIO issues.

![Gateway and node topology](assets/ptmp-topology.svg)

## Channel access policy implemented in the gateway

The gateway uses a single radio-owner FreeRTOS task and bounded queues. It maintains per-node session, replay and request states, stores outgoing downlink sequences before transmission, and schedules TX with a shared airtime budget. This provides deterministic ownership of the radio even when HTTP requests, archive service and incoming telemetry happen concurrently. ACK frames get the highest queue priority because the node is already waiting for confirmation. Only lower-priority POLL frames are subject to **SX1262 Channel Activity Detection (CAD)**, using the non-blocking RadioLib 7.8.1 `startChannelScan()` and `getChannelScanResult()` API.

For a POLL, the gateway first enters CAD instead of transmitting immediately. If the radio reports `RADIOLIB_CHANNEL_FREE`, the gateway starts TX; a detected LoRa signal causes it to resume RX and requeue the exact same POLL, keeping the already-reserved downlink sequence and session. The retry uses a bounded random delay supplied by the ESP32 hardware RNG. The contention window increases with consecutive busy-channel observations and stops growing beyond a configured limit. Repeated busy results eventually hit the original request deadline and report **unavailable**, not success. A radio CAD timeout or driver fault triggers error reporting and normal driver recovery. All states share a single SPI owner. No extra task drives the SX1262 concurrently.

![Gateway POLL and telemetry sequence](assets/poll-sequence.svg)

The constants are defined in `firmware/core/channel_access.h`: CAD watchdog 1,500 ms, base deferral 300 ms, randomized window up to 12,000 ms, capped exponential growth. The node's own HELLO and telemetry transmissions are still controlled by its *unchanged* uplink scheduler, initial randomized contention and ACK retries. Gateway CAD does **not** sense every hidden sender and does **not** coordinate node-to-node uplink starts. It can reduce gateway-generated interference with ongoing LoRa activity, not remove simultaneous uplink collisions. It is not TDMA, LoRaWAN, listen-before-talk regulatory certification, or a guarantee of availability.

## Traffic priorities and backpressure

| Priority | Activity | Policy |
| --- | --- | --- |
| Highest | ACK for authenticated HELLO, TELEMETRY or CHECKPOINT | Queue first, coalesce identical ACKs, transmit with shared duty budget; skip CAD |
| Next | Session challenge POLL and manual refresh/checkpoint request | Reserve unique sequence in flash, wait for radio gate, use CAD and bounded backoff |
| Lowest | Automatic stale-data POLL | Only one outstanding request, round-robin inventory, after missed-report grace period; use CAD and randomized backoff |

The software airtime gate is shared by all gateway downlinks: it delays the next TX by roughly 100 times the previous packet airtime, implementing approximately a 1% pacing budget. At the nominal SF7/BW125/CR4/5 settings, the node protocol reference calculates an ACK of 41 bytes at approximately 87.3 ms. Four hourly uplinks therefore need approximately 349 ms of gateway ACK airtime each hour before retries and checkpoints, far below 36 seconds/hour of a 1% airtime budget. This calculation is **not a throughput guarantee**. At high spreading factors, heavy retry load or simultaneous archives, the capacity and ACK latency change significantly. Even a low average airtime cannot guarantee that two randomly aligned node packets never overlap. Configure the node transmission interval and RF parameters for the actual site and leave substantial budget for retries and maintenance requests.

No gateway-side code can stop two autonomous node uplinks from colliding when both nodes cannot hear each other. A true slot-based scheduler would require protocol changes to the ESP32-S3 nodes: trustworthy time synchronization, periodic slot assignment, drift guard bands, loss recovery and an authenticated schedule version. Those changes are **out of scope** because the node repository must remain untouched. The present solution uses CAD/backoff on optional downlinks, node randomized retries, air-time budgeting, queue limits, and deadline-aware request cancellation, without misrepresenting these as TDMA.

## Monitoring and acceptance

The status API reports `cadBusy`, `cadClear`, `cadErrors` and `cadDeferred` counters alongside accepted packets, duplicate RX, authentication errors, queue drops, timeouts, RSSI/SNR and transmit load. These numbers describe gateway observations. A low CAD-busy count **does not prove** no collisions occurred, because a collided uplink may never be decoded by the receiver. A valid but old kWh value is retained and labelled stale; a correlated POLL response with the energy-valid flag cleared is not a fresh measurement.

A deployment test should include four active nodes with their real source meter profiles, one gateway and matched antennas at intended panel locations. Record a one-hour baseline of autonomous telemetry, then deliberately overlap requests and checkpoint retrieval, monitor TX/RX counters and timestamped request completion, reboot an active node, interrupt gateway power and verify durable archive recovery. Perform controlled temporary RF attenuation and one-node power-off tests. Measure each node's report success ratio, worst-case response latency, and effective duty consumption at the selected SF/BW/power before accepting the topology. Interference and building walls must be validated in the factory; a native simulator or successful PlatformIO build cannot prove RF reliability.

## Indonesia frequency and airtime constraints

The upstream node and this gateway currently share a legacy **915 MHz factory software default**, preserved only for source-level compatibility. In Indonesia do not enable transmitter operation on that default: select an allowed carrier such as 921 MHz **locally on both devices**. The [2025 Indonesian LPWAN class-license regulation](RF_COMPLIANCE_ID.md) lists 920–923 MHz and a 1% uplink and downlink duty cycle. The local radio pacing does not replace certification, antenna-EIRP calculations or a power-cycle duty audit. Do not assume node support for BW500 means that bandwidth is legal at the deployment site.
