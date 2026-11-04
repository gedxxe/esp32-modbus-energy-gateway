# Midea Engineering — Gateway Monitoring Daya LoRa

[English](README.md) · [Desain komunikasi dan penjadwalan](docs/RADIO_DESIGN.md) · [Persiapan perangkat](docs/COMMISSIONING.md) · [Kompatibilitas node](docs/NODE_COMPATIBILITY.md)

Repository ini berisi firmware penerima untuk [node ESP32 pembaca power meter](https://github.com/gedxxe/esp32-modbus-energy). Pembacaan register Modbus, validasi data, serta perhitungan rasio PT dan CT tetap dilakukan pada node. Gateway ESP32-C3 bertugas menerima paket LoRa yang sudah terautentikasi, menyimpan data yang memang perlu diarsipkan, dan menyediakan halaman monitoring melalui jaringan lokal. Acuan format paketnya adalah **Energy Station protocol v1** yang digunakan firmware node, bukan format JSON atau HTTP yang dikirim langsung lewat radio.

## Monitoring kWh dan refresh manual

Nilai kWh yang tampil merupakan pembacaan valid terakhir dari power meter. Ketika belum ada kiriman baru, angka terakhir tetap terlihat bersama informasi waktu pengukuran, umur data, ID meter, dan kondisi koneksi LoRa. Statusnya berubah menjadi **data lama** supaya operator tidak salah menganggap angka tersebut sebagai pembacaan real-time. Jika pembacaan gagal atau sejak awal memang belum ada data valid, gateway menampilkan tanda kosong, bukan angka nol. Nilai kWh yang diterima sudah dalam satuan engineering; gateway tidak mengalikan CT/PT untuk kedua kalinya.

Tombol **Request live** digunakan untuk meminta telemetry terbaru yang tersedia pada satu node. Perintah ini tidak memaksa node menjalankan transaksi pembacaan register Modbus RS485 baru. Gateway mengirim paket `POLL` mode 0, kemudian menunggu paket `TELEMETRY` dengan nomor korelasi yang sesuai. Mekanismenya mirip permintaan baca melalui HTTP GET, tetapi paket di udara tetap mengikuti protokol LoRa v1 milik node. Hasil klik tidak otomatis berarti data berhasil diterima: ada tahap antrean, pengiriman, menunggu balasan, dan kemungkinan timeout. Tombol checkpoint memakai `POLL` mode 1 untuk meminta arsip satu hari; datanya diverifikasi sebelum disimpan ke flash gateway.

## Penjadwalan beberapa node

Interval pengiriman berkala diatur dari firmware atau halaman konfigurasi masing-masing node. Misalnya empat node sama-sama memakai interval satu jam, waktu kirimnya belum tentu berbeda karena perangkat memiliki jam, waktu boot, dan kondisi radio masing-masing. Gateway tidak boleh menganggap interval tersebut sebagai jadwal TDMA. Sistem menggunakan prioritas ACK, pembatasan airtime, urutan pengiriman bergiliran, retry dari sisi node, dan jeda antarpoll otomatis. Bila data valid tidak diterima selama satu jam ditambah waktu toleransi, gateway dapat mengirim permintaan penyegaran berprioritas rendah. Permintaan gagal tidak langsung dikirim berulang-ulang. Detailnya dijelaskan pada [dokumen desain radio](docs/RADIO_DESIGN.md), termasuk hal-hal yang tetap harus diuji menggunakan perangkat fisik.

Setiap node mempunyai kombinasi network ID, node ID, dan HMAC key tersendiri. Kapasitas inventori firmware saat ini adalah 16 node, tetapi jumlah itu tidak berarti 16 perangkat pasti dapat berkomunikasi tanpa gangguan di dalam pabrik. Kualitas antena, kondisi tembok, konfigurasi SF/bandwidth, lalu lintas paket, dan airtime perlu diuji di lokasi. HMAC digunakan untuk autentikasi, bukan mengenkripsi nilai energi. Karena itu, data sensitif maupun kredensial perusahaan tidak boleh dimasukkan ke payload radio atau repository.

## Perangkat, build, dan pengujian

Acuan awal gateway adalah **ESP32-C3-DevKitM-1** dengan modul **SX1262 Ra-01SH V1.1**. Pastikan jalur SPI, DIO1, BUSY, RESET, pengaturan RF switch, dan jenis oscillator sesuai board yang benar-benar dibeli. Saat awal digunakan, radio masih nonaktif dan daftar node kosong. Jaringan AP commissioning serta kode akses halaman web dicetak ke Serial Monitor pada 115200 baud. Alamat dashboard lokal adalah `http://192.168.4.1`. Kata sandi AP pemulihan node adalah konfigurasi tersendiri dalam repository node dan tidak diubah oleh pekerjaan gateway ini.

Perintah build dari root repository, menggunakan Python, PlatformIO, dan compiler C++:

```sh
python -m platformio run -e esp32c3
python tools/test_native.py
```

Untuk membangun dashboard lokal, masuk ke direktori `dashboard/`, kemudian jalankan `npm ci`, `npm run build`, dan `npm test` dengan Node.js 22. Aset hasil build dimasukkan ke firmware melalui `firmware/web/assets.h`, sehingga firmware sebaiknya dikompilasi setelah dashboard dibangun. Jangan mengunggah filesystem image baru ke perangkat berisi arsip pengukuran tanpa memeriksa konsekuensi penghapusan datanya.

## Batasan implementasi

Sistem ini dirancang sebagai alat monitoring lokal, bukan layanan yang dibuka langsung ke Internet. Autentikasi dashboard masih menggunakan kode commissioning, komunikasi web masih HTTP, dan gateway belum mempunyai RTC tepercaya ataupun integrasi database pusat. Uji kode dan simulasi paket bisa membuktikan konsistensi logika, tetapi belum menggantikan pengujian LoRa, pembacaan meter asli, serta jangkauan pada lingkungan pabrik. Lihat [panduan commissioning](docs/COMMISSIONING.md), [hasil verifikasi](docs/VERIFICATION.md), dan [aturan kebersihan repository](tools/security_audit.py) sebelum menerapkan build baru.
