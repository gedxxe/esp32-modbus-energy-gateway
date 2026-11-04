# Midea Engineering — Gateway Monitoring Daya LoRa

[English](README.md) · [PlatformIO / VS Code](docs/PLATFORMIO_VSCODE.md) · [Wiring ESP32-C3](docs/HARDWARE.md) · [Pengendalian collision](docs/COLLISION_CONTROL.md) · [Kompatibilitas node](docs/NODE_COMPATIBILITY.md)

Repository ini berisi firmware penerima untuk [node ESP32 pembaca power meter](https://github.com/gedxxe/esp32-modbus-energy). Pembacaan register Modbus, validasi data, serta perhitungan rasio PT dan CT tetap dilakukan pada node. Gateway ESP32-C3 bertugas menerima paket LoRa yang sudah terautentikasi, menyimpan data yang memang perlu diarsipkan, dan menyediakan halaman monitoring melalui jaringan lokal. Acuan format paketnya adalah **Energy Station protocol v1** yang digunakan firmware node, bukan format JSON atau HTTP yang dikirim langsung lewat radio.

![Topologi PTMP node ESP32-S3 dan gateway ESP32-C3](docs/assets/ptmp-topology.svg)

Gateway yang digunakan sementara adalah **ESP32-C3-DevKitM-1**, bukan ESP32-C6 yang disebut dalam beberapa dokumen node terdahulu. Repository node tetap dipakai sebagai referensi dan tidak diubah.

## Monitoring kWh dan refresh manual

Nilai kWh yang tampil merupakan pembacaan valid terakhir dari power meter. Ketika belum ada kiriman baru, angka terakhir tetap terlihat bersama informasi waktu pengukuran, umur data, ID meter, dan kondisi koneksi LoRa. Statusnya berubah menjadi **data lama** supaya operator tidak salah menganggap angka tersebut sebagai pembacaan real-time. Jika pembacaan gagal atau sejak awal memang belum ada data valid, gateway menampilkan tanda kosong, bukan angka nol. Nilai kWh yang diterima sudah dalam satuan engineering; gateway tidak mengalikan CT/PT untuk kedua kalinya.

Tombol **Request live** digunakan untuk meminta telemetry terbaru yang tersedia pada satu node. Perintah ini tidak memaksa node menjalankan transaksi pembacaan register Modbus RS485 baru. Gateway mengirim paket `POLL` mode 0, kemudian menunggu paket `TELEMETRY` dengan nomor korelasi yang sesuai. Mekanismenya mirip permintaan baca melalui HTTP GET, tetapi paket di udara tetap mengikuti protokol LoRa v1 milik node. Hasil klik tidak otomatis berarti data berhasil diterima: ada tahap antrean, pengiriman, menunggu balasan, dan kemungkinan timeout. Tombol checkpoint memakai `POLL` mode 1 untuk meminta arsip satu hari; datanya diverifikasi sebelum disimpan ke flash gateway.

![Alur request/response POLL dan TELEMETRY melalui LoRa](docs/assets/poll-sequence.svg)

## Penjadwalan beberapa node

Interval pengiriman berkala diatur dari firmware atau halaman konfigurasi masing-masing node. Misalnya empat node sama-sama memakai interval satu jam, waktu kirimnya belum tentu berbeda karena perangkat memiliki jam, waktu boot, dan kondisi radio masing-masing. Gateway tidak boleh menganggap interval tersebut sebagai jadwal TDMA. Sistem menggunakan prioritas ACK, pembatasan airtime, urutan pengiriman bergiliran, randomized backoff dari sisi node, serta Channel Activity Detection (CAD) non-blocking sebelum gateway mengirim POLL. Jika kanal sibuk, POLL ditunda memakai nomor urut dan sesi yang sama sehingga tidak menciptakan request duplikat. Bila data valid tidak diterima selama satu jam ditambah waktu toleransi, gateway dapat mengirim permintaan penyegaran berprioritas rendah. Permintaan gagal tidak langsung dikirim berulang-ulang. Detailnya dijelaskan pada [dokumen desain radio](docs/RADIO_DESIGN.md), termasuk hal-hal yang tetap harus diuji menggunakan perangkat fisik.

Setiap node mempunyai kombinasi network ID, node ID, dan HMAC key tersendiri. Kapasitas inventori firmware saat ini adalah 16 node, tetapi jumlah itu tidak berarti 16 perangkat pasti dapat berkomunikasi tanpa gangguan di dalam pabrik. Kualitas antena, kondisi tembok, konfigurasi SF/bandwidth, lalu lintas paket, dan airtime perlu diuji di lokasi. HMAC digunakan untuk autentikasi, bukan mengenkripsi nilai energi. Karena itu, data sensitif maupun kredensial perusahaan tidak boleh dimasukkan ke payload radio atau repository.

Metode tersebut mengurangi collision dari sisi downlink gateway, tetapi tidak dapat menjamin dua uplink node yang tidak saling mendengar akan bebas collision. Penjadwalan TDMA yang benar memerlukan perubahan protokol serta sinkronisasi waktu pada kedua firmware; itu tidak dilakukan karena repository node harus tetap utuh. Rincian batasan dan pengujiannya ada di [dokumen pengendalian collision](docs/COLLISION_CONTROL.md).

## Perangkat, build, dan pengujian

![Wiring gateway ESP32-C3 dan modul SX1262 Ra-01SH](docs/assets/wiring-c3.svg)

![Struktur root project PlatformIO di VS Code](docs/assets/platformio-layout.svg)

Gateway menggunakan `platformio.ini` pada root repository, environment `esp32c3`, dan board `esp32-c3-devkitm-1`. Versi pioarduino `55.03.311`, RadioLib `7.8.1`, dan ArduinoJson `7.3.1` sengaja sama dengan versi package node ESP32-S3. Ikuti [panduan PlatformIO/VS Code](docs/PLATFORMIO_VSCODE.md) supaya tidak salah membuka folder `firmware/` sebagai project root.

Acuan awal gateway adalah **ESP32-C3-DevKitM-1** dengan modul **SX1262 Ra-01SH V1.1**. Pastikan jalur SPI, DIO1, BUSY, RESET, pengaturan RF switch, dan jenis oscillator sesuai board yang benar-benar dibeli. Saat awal digunakan, radio masih nonaktif dan daftar node kosong. Jaringan AP commissioning serta kode akses halaman web dicetak ke Serial Monitor pada 115200 baud. Alamat dashboard lokal adalah `http://192.168.4.1`. Kata sandi AP pemulihan node adalah konfigurasi tersendiri dalam repository node dan tidak diubah oleh pekerjaan gateway ini.

Perintah build dari root repository, menggunakan Python, PlatformIO, dan compiler C++:

```sh
python tools/check_compatibility.py
python tools/check_wiring.py
python tools/test_native.py
python -m platformio run -e esp32c3
```

Untuk membangun dashboard lokal, masuk ke direktori `dashboard/`, kemudian jalankan `npm ci`, `npm run build`, dan `npm test` dengan Node.js 22. Aset hasil build dimasukkan ke firmware melalui `firmware/web/assets.h`, sehingga firmware sebaiknya dikompilasi setelah dashboard dibangun. Jangan mengunggah filesystem image baru ke perangkat berisi arsip pengukuran tanpa memeriksa konsekuensi penghapusan datanya.

### Frekuensi radio untuk Indonesia

**Jangan langsung mengaktifkan transmitter pada frekuensi bawaan 915 MHz** untuk instalasi LPWAN di Indonesia. Dalam ketentuan izin kelas 2025, pita LPWAN tercantum pada 920–923 MHz dengan duty cycle maksimum 1% untuk uplink dan downlink. Atur frekuensi yang sama, misalnya 921 MHz, pada pengaturan lokal node dan gateway sebelum radio diaktifkan; repository node tidak perlu diubah. Daya EIRP termasuk pengaruh gain antena dan loss kabel, sehingga angka TX power di firmware saja tidak cukup untuk menyatakan alat patuh regulasi. Lihat [checklist RF Indonesia](docs/RF_COMPLIANCE_ID.md).

## Batasan implementasi

Sistem ini dirancang sebagai alat monitoring lokal, bukan layanan yang dibuka langsung ke Internet. Autentikasi dashboard masih menggunakan kode commissioning, komunikasi web masih HTTP, dan gateway belum mempunyai RTC tepercaya ataupun integrasi database pusat. Uji kode dan simulasi paket bisa membuktikan konsistensi logika, tetapi belum menggantikan pengujian LoRa, pembacaan meter asli, serta jangkauan pada lingkungan pabrik. Lihat [panduan commissioning](docs/COMMISSIONING.md), [hasil verifikasi](docs/VERIFICATION.md), dan [aturan kebersihan repository](tools/security_audit.py) sebelum menerapkan build baru.

### Paket firmware dan keamanan flash

Setelah build firmware dan image LittleFS khusus instalasi awal, jalankan `python tools/package.py` lalu `python tools/package.py --verify` untuk membuat paket ZIP yang diverifikasi menggunakan SHA256. File ELF tidak disertakan dalam paket publik karena berpotensi memuat path folder dari komputer yang mengompilasi firmware. Image `littlefs.bin` **hanya untuk provisioning pertama**, bukan update rutin. Jangan sampai update firmware menghapus partisi `gw_nvs` yang menyimpan ledger sequence LoRa atau partisi `archives` yang menyimpan checkpoint. ZIP hasil CI tersedia sebagai workflow artifact dan tidak dimasukkan ke commit repository.
