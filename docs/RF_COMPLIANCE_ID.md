<p align="left"><img src="assets/midea-brand.svg" alt="Midea" width="185" /></p>

![SX1262](assets/badges/radio.svg) ![C3](assets/badges/board.svg) ![PTMP](assets/badges/protocol.svg)

# Radio parameters, antenna gain and regional reference

[← README](../README.md) · [Hardware](HARDWARE.md) · [Collision control](COLLISION_CONTROL.md) · [Wiring](assets/wiring-c3.svg)

Both ESP32-S3 node and ESP32-C3 gateway ship with an **unchanged 915 MHz software default**. Each firmware initially configures its SX1262 at **14 dBm TX power** with 125 kHz bandwidth, SF7, CR4/5, preamble 8 and sync 0x12. These defaults are identical for protocol interoperability. They do not certify any specific antenna, country-specific band or final emitted field strength.

<details open>
<summary><b>RF power and your Moxon antenna</b></summary>

Transmitter **conducted output power** is not equivalent to **EIRP** (effective isotropic radiated power). Use:

```text
EIRP (dBm) = radio TX power (dBm) + antenna gain (dBi) − cable/connectors loss (dB)
```

Illustration only: 14 dBm conducted + 6 dBi antenna − 1 dB feedline loss = **19 dBm EIRP**. Actual Moxon gain, VSWR, cable loss, mounting, regulatory permissions and emitted power must be verified, not inferred from the firmware setting. The source validation allows **0–22 dBm** conducted TX settings, but that range is a radio-driver/configuration bound, **not** a declaration that every setting is appropriate for the attached antenna or radio jurisdiction.

</details>

<details>
<summary><b>Indonesia LPWAN class-license reference</b></summary>

[Permenkomdigi Nomor 2 Tahun 2025, Lampiran II](https://peraturan.bpk.go.id/Download/378118/permenkomdigi-no-2-tahun-2025.pdf) describes the following 920–923 MHz LPWAN category (read the complete regulation and current equipment requirements):

| Parameter | Reference in regulation |
| --- | --- |
| LPWAN frequency band | 920–923 MHz |
| Max bandwidth per channel | 250 kHz |
| Duty cycle | 1% uplink, 1% downlink |
| Gateway EIRP | Maximum 400 mW / 26 dBm |
| Node EIRP | Maximum 100 mW / 20 dBm |

The **915 MHz software default remains unchanged** in both repositories as requested. This table is a regional regulatory reference, **not** a firmware frequency override, an authorization for transmission outside the listed class, or proof of equipment certification.

</details>

<details>
<summary><b>PTMP site acceptance</b></summary>

With the node and gateway on the same permitted PHY, record actual conducted power, antenna and connector gain/loss, observed RSSI/SNR, packet loss, retransmissions, CAD busy/clear counts, archive transfer time and duty cycle across restarts. SX1262 supports half-duplex communication; a Moxon antenna's radiation pattern can improve coverage in a preferred direction but cannot prevent two mutually hidden nodes from transmitting at the same time.

</details>

The gateway radio remains **disabled until commissioned**, with the initial inventory empty. HMAC keys, node IDs and gateway IDs must be configured per installation. Do not expose the gateway's local commissioning HTTP service to the public Internet.
