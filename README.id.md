# ESP32 Modbus Energy — Gateway LoRa

Firmware gateway **ESP32-C3 + SX1262** untuk menerima data power meter dari [firmware node ESP32-S3](https://github.com/gedxxe/esp32-modbus-energy).

Repository ini saat ini menyediakan **access point Wi-Fi lokal dan dashboard**. Belum ada koneksi Wi-Fi STA, pengiriman ke PostgreSQL, atau integrasi ke website Midea. Heltec ESP32-S3 SX1262 V3 juga belum menjadi target build gateway ini.

## Alur data

```text
Power meter → RS485/Modbus → Node ESP32-S3 → SX1262 LoRa
           → Gateway ESP32-C3 → Wi-Fi AP lokal → Browser
```

Node membaca dan menghitung nilai kWh berdasarkan konfigurasi meter. Gateway memverifikasi paket LoRa, menangani session, mengirim ACK, menampilkan data terakhir, dan menyimpan checkpoint yang diminta ke LittleFS. **Gateway tidak mengalikan CT/PT lagi.**

[Diagram topologi](docs/assets/ptmp-topology.svg) · [Protokol radio](docs/NODE_COMPATIBILITY.md) · [Arsitektur firmware](docs/ARCHITECTURE.md)

## Build

Buka folder **`firmware/`** melalui VS Code dengan PlatformIO, bukan folder root repository.

| Parameter | Firmware saat ini |
| --- | --- |
| Board | `esp32-c3-devkitm-1` (ESP32-C3, flash 4 MB) |
| Radio | SX1262 / Ra-01SH V1.1 |
| Platform | pioarduino `55.03.311` |
| RadioLib / ArduinoJson | `7.8.1` / `7.3.1` |
| Protokol LoRa | Energy Station v1 |
| PHY default | 915 MHz, BW125, SF7, CR4/5, preamble 8, sync 0x12, TX 14 dBm |
| Inventori | Maksimal 16 entri node; bukan jaminan kapasitas RF |

Dari root repository:

```powershell
python -m platformio run --project-dir firmware -e esp32c3
python tools/test_native.py
python tools/check_compatibility.py
```

Source dashboard berada di folder `dashboard/`. Jika UI diubah, buat ulang aset yang disematkan ke firmware **sebelum** build:

```powershell
cd dashboard
npm ci
npm run build
npm test
```

Lihat [panduan PlatformIO](docs/PLATFORMIO_VSCODE.md) dan [pengaturan gateway](docs/COMMISSIONING.md) untuk proses upload dan konfigurasi awal.

## Cara penggunaan

1. Hubungkan laptop atau HP ke Wi-Fi AP gateway. Password dan kode akses tampil di Serial Monitor (115200 baud) saat boot.
2. Tambahkan setiap node dengan Network ID, Node ID, dan kunci 32-byte yang cocok. Samakan parameter radio dengan node, lalu aktifkan gateway.
3. Pastikan session node sudah terkonfirmasi. **Request live** meminta pengukuran terakhir yang tersedia pada node, bukan memicu pembacaan Modbus baru.
4. Gunakan **Request checkpoint** untuk meminta arsip tanggal tertentu. ACK berarti paket diterima, **bukan** bukti penulisan flash berhasil.

Nilai kWh valid terakhir tetap dapat terlihat ketika paket berikutnya tidak valid atau belum diterima. Periksa umur datanya; data hilang bukan berarti nol.

## Batas sistem

- Semua node berbagi satu kanal LoRa half-duplex. CAD dan random backoff mengurangi risiko collision tertentu, tetapi tidak menjamin bebas collision.
- Gateway memakai pembatasan TX airtime 1% per boot. Jumlah node dan interval pengiriman yang rapat dapat menyebabkan antrean ACK.
- Penyimpanan lokal maksimal **64 record checkpoint per gateway**, tanpa penghapusan otomatis.
- Ledger sequence POLL maksimal **64 identitas node/session**. Jika penuh atau rusak, POLL ditolak. Jangan menghapus ledger untuk melewati perlindungan replay.
- Saat update aplikasi, partisi `gw_nvs` dan arsip harus tetap utuh. **Jangan jalankan `uploadfs` pada perangkat yang memiliki arsip penting.**
- HMAC digunakan untuk autentikasi, bukan enkripsi. Dashboard HTTP lokal tidak ditujukan untuk akses Internet publik.
- Frekuensi 915 MHz adalah default firmware, bukan izin penggunaan frekuensi di lokasi pemasangan.

CI saat ini menguji build gateway serta revisi client yang dipin. Hasil tersebut bukan bukti kompatibilitas dengan semua commit client terbaru atau performa radio di pabrik. Lihat [status pengembangan](WORK_STATUS.md), [pengujian](docs/VERIFICATION.md), [mekanisme LoRa](docs/COLLISION_CONTROL.md), dan [referensi RF](docs/RF_COMPLIANCE_ID.md).

[English README](README.md) · [Lisensi](LICENSE)
