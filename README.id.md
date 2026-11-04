# Gateway LoRa Energy Station

[English](README.md) · [Commissioning](docs/COMMISSIONING.md) · [Kompatibilitas](docs/NODE_COMPATIBILITY.md) · [Tracker](WORK_STATUS.md)

Firmware gateway PTMP raw-LoRa pada repository terpisah, kompatibel dengan node `esp32-modbus-energy` firmware3.4.3 / protokol **Energy Station v1**, commit26c3a9429c8c4d410ef11a1b81e1e42f295034b1. Target pertama sesuai koreksi pemilik adalah **ESP32-C3**, memakai board referensi DevKitM-1 4 MB tanpa PSRAM dan SX1262 Ra-01SH V1.1. Model board fisik pengguna belum diketahui. Pin dan pilihan oscillator/RF switch dipisahkan dari protokol.

Implementasi mencakup codec/HMAC, inventori16 node, ACK prioritas dan fairness, gate airtime gateway1%, kandidat sesi yang dibuktikan lewat live POLL, reservasi sequence yang diverifikasi sebelum TX, telemetry32 byte, reassembly446 byte schema3, validasi CRC/semantik, serta arsip lokal immutable. Dashboard **React** ringan memakai aset lokal terkompresi, dengan bahasa Inggris/Indonesia dan akses AP. Tidak ada cloud, database server, Enterprise Wi-Fi, LoRaWAN, mesh, OTA atau perintah remote konfigurasi node.

## Build dan pengujian

UI0.1.1 memakai grid card per `(Network ID, Node ID)`, sesuai sketsa: status sesi/data, kWh, umur data, RSSI/SNR, frame/sequence terakhir dan counter RX per node. Buka **Received data** untuk detail payload yang sudah didecode. Card **+ Add node / Tambah node** memilih saran ID yang belum dipakai, memvalidasi duplikasi, lalu menyimpan inventori ke flash NVS lewat verified save. ID/nama/key tersimpan; data live dan counter link tetap di RAM. Indikator fresh/stale terpisah dari sesi/link, dan umur data terus berjalan saat koneksi browser terputus. [Preview card](docs/assets/node-cards-mobile-qa.png) memakai data simulasi.

Host terminal **INPUT**, dari root repository (Python3 + PlatformIO Core6.1.19, g++ untuk tes native):

```powershell
python -m platformio run -e esp32c3
python -m platformio run -e esp32c3 -t buildfs
python tools/test_native.py
```

Di workstation ini PlatformIO berada di `C:/Users/Lenovo/.platformio/penv/Scripts/platformio.exe`; gunakan executable tersebut jika Python aktif belum memiliki modul PlatformIO. Framework/dependency dipin pada platformio.ini. Aset web hasil build sudah disertakan. Untuk mengubah UI, host terminal **INPUT** dari `dashboard/` (Node22):

```powershell
npm ci
npm run build
npm test
```

Compile firmware setelah membangun ulang UI. `npm run preview` memakai data simulasi berlabel untuk QA tampilan, bukan bukti RF. Lihat [hasil aktual dan ukuran compile](docs/VERIFICATION.md), manifest release, dan `python tools/package.py --verify` untuk pemeriksaan paket. Jangan mengunggah image filesystem saat update aplikasi rutin karena arsip akan terganti.

## Commissioning

Radio default nonaktif dan inventori kosong. ID gateway/network default1; contoh ID node2/3/4 harus memakai key masing-masing. Verifikasi [wiring C3](docs/HARDWARE.md), suplai3,3 V dan antena sebelum mengaktifkan radio. Serial Monitor **OUTPUT**,115200 baud, mencetak SSID AP, password WPA2 dan kode commissioning baru tiap boot. Sambungkan ke AP dan buka192.168.4.1. Masukkan key baru pada inventori lokal, simpan, cocokkan PHY dengan node, lalu aktifkan radio. Key tersimpan tidak dikembalikan melalui API/log/browser storage. NVS belum dienkripsi; HMAC memberikan autentikasi tanpa enkripsi payload.

Bedakan konfigurasi tersimpan, chip initialized, HELLO ACK terkirim, sesi terkonfirmasi dan data fresh. HELLO replay tidak boleh mengganti sesi established tanpa respons live POLL dengan correlation baru. ID Modbus bukan ID radio node. Mengubah inventori tidak mengubah node dari jarak jauh dan tidak mengubah identitas pada arsip lama.

Telemetry live ada di RAM; arsip checkpoint durable disimpan pada64 slot immutable. Sepuluh titik WIB dan metadata historis dipertahankan; kWh tidak dikalikan CT/PT lagi. Nilai invalid/stale/missing tampil `—`, bukan nol. UTC gateway tidak tersedia; receive timestamp memakai monotonic dan boot identity terpisah dari UTC node.

Ikuti [panduan acceptance dan recovery](docs/COMMISSIONING.md): satu node → tiga node → kapasitas. Enam belas slot inventori bukan jaminan kapasitas RF. Sequence POLL harus meningkat dalam sesi node yang sama termasuk setelah gateway reboot. Ledger rusak/hilang menghentikan POLL; pemulihan membutuhkan tindakan lokal dan rotasi sesi node, bukan reset remote buatan.

Build dan tes software dipisahkan dari **AWAITING_HARDWARE**: wiring fisik, pairing RF, brownout, tiga node, jangkauan dan kapasitas belum dianggap lolos tanpa bukti. Lihat [diagram kepemilikan task](docs/ARCHITECTURE.md), [keputusan](docs/DECISIONS.md), AGENTS.md dan tracker sebelum melanjutkan pekerjaan. Lisensi GPL-3.0-only beserta provenance node dipertahankan.
