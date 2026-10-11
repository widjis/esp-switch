# ESP Switch

Firmware ESP8266 BA129 untuk mengendalikan daya HDMI splitter dari backend rental PlayStation. HTTP API mengatur sesi; timer lokal mematikan relay saat waktu habis walaupun Wi-Fi/server terputus. OTA tersedia untuk pemeliharaan saat tidak ada sesi aktif.

Status: firmware 0.1.1 sudah di-flash ke BA129 dan tersambung Wi-Fi. HTTP sesi/timer dan upload OTA sudah diuji pada perangkat; lihat [laporan bench test](docs/bench-test.md). Kontak relay secara fisik, pemutusan gambar HDMI, ketepatan timer saat client lambat, dan perilaku saat jaringan putus masih perlu diuji.

## Perangkat

- BA129 ESP-12F/ESP8266EX, flash 4 MB; relay GPIO5 HIGH=ON dan LED GPIO16 LOW=ON sesuai manual board.
- C3 bridge: GPIO21 TX -> BA129 RX; GPIO20 RX <- BA129 TX; GND bersama. BA129 memakai microUSB 5 V selama pengujian; semua kabel AC dilepas. Jangan menghubungkan pin 5 V antarboard.
- Bridge yang sudah diuji memakai UART **57600 baud**, buffer USB RX 16 KB, TX 4 KB. Upload stub dan pembacaan 64 KB berhasil; baud 115200 dengan buffer baru belum diuji.
- Kendalikan daya splitter, bukan mematikan PlayStation. Jika splitter memakai adaptor DC, uji pemutusan daya setelah adaptor. Pastikan splitter tidak tetap melewatkan gambar atau mendapat daya dari HDMI.

## Build dan konfigurasi

```sh
python3 -m venv .venv
.venv/bin/pip install platformio==6.2.0
python3 scripts/configure.py
.venv/bin/pio run -e ba129
.venv/bin/pio test -e native
```

`scripts/configure.py` meminta Wi-Fi dan membuat token API/password OTA acak di `include/secrets.h`. File itu diabaikan Git. Simpan kredensial pada backend. Alternatif: salin `include/secrets.example.h` ke `include/secrets.h` lalu isi sendiri. Token API minimal 32 karakter; password OTA minimal 16 karakter. Build tanpa konfigurasi tetap bisa dikompilasi tetapi relay tetap OFF dan jaringan tidak diaktifkan.

Versi awal memakai konfigurasi saat build. Halaman setup Wi-Fi dari HP belum diimplementasikan. Tidak ada fallback access point terbuka.

Dependensi dipatok di `platformio.ini`. Layout flash 4 MB/FS 1 MB menyisakan ruang OTA; ukuran firmware baru tetap harus sesuai ruang yang tersedia. Binary: `.pio/build/ba129/firmware.bin`.

## Flash pertama dengan C3

IO0 BA129 -> GND, lalu tekan RESET BA129. C3 tetap menjalankan firmware bridge.

```sh
.venv/bin/pio run -e ba129 -t upload --upload-port /dev/cu.usbmodem11401
```

Port dapat berubah. Setelah berhasil, lepas IO0–GND dan tekan RESET BA129. Log serial 57600 menampilkan IP setelah Wi-Fi tersambung. Perintah upload menulis firmware aplikasi dan mengganti firmware lama; backup 64 KB sebelumnya bukan backup penuh 4 MB. Jangan meng-upload jika ingin mempertahankan firmware pabrik tanpa backup lengkap.

Pada bench test, uploader esptool 3 bawaan PlatformIO gagal saat stub upload. Jalur yang berhasil memakai **esptool 5.3.1** terpasang terpisah, setelah IO0–GND dan RESET:

```sh
esptool --chip esp8266 --port /dev/cu.usbmodem11401 --baud 57600 --before no-reset --after no-reset write-flash --flash-mode dio --flash-freq 40m --flash-size 4MB 0x0 .pio/build/ba129/firmware.bin
```

Perintah `esptool` memerlukan instalasi versi 5 pada PATH; versi itu tersedia dari paket Arduino ESP32 pada laptop pengujian. PlatformIO uploader lama belum diperbaiki. Sesudah penulisan, lepas IO0–GND dan RESET untuk menjalankan aplikasi.

## HTTP API

Semua endpoint memerlukan `Authorization: Bearer <API_TOKEN>`. JSON maksimum 512 byte. Tidak ada CORS untuk browser pelanggan. Gunakan backend sebagai pengendali tunggal dan jaringan pengelola terpisah; HTTP tidak mengenkripsi token. Jangan membuka endpoint ke internet atau jaringan pelanggan. OTA password bukan verifikasi tanda tangan firmware; signed OTA belum diimplementasikan.

| Endpoint | Fungsi |
|---|---|
| `GET /status` | Versi, boot ID, relay, sesi, revision, generation, sisa waktu, IP, RSSI |
| `POST /session/start` | Mulai sesi baru; revision harus 1 |
| `POST /session/update` | Tetapkan sisa waktu baru; revision harus meningkat |
| `POST /session/stop` | Matikan sesi; revision harus meningkat |
| `POST /maintenance/ota` | Buka/tutup jendela OTA 5 menit saat idle |

Pertama baca `/status`, lalu gunakan `boot_id` pada setiap mutasi. Start juga memerlukan `expected_generation` sesuai `generation` dari status. Ini mencegah perintah lama membuka sesi setelah sesi lain atau restart. Backend harus memakai ID sesi unik dan menyimpan revision/payload sebelum mengirim. Jika request timeout, ulangi payload yang sama persis; jangan menghitung durasi baru saat retry.

Contoh start (nilai boot/generation diperoleh dari status):

```json
{"boot_id":"BOOT_ID","expected_generation":0,"session_id":"rental-001","revision":1,"remaining_seconds":3600}
```

Update memakai ID sesi sama dan revision baru:

```json
{"boot_id":"BOOT_ID","session_id":"rental-001","revision":2,"remaining_seconds":1800}
```

`remaining_seconds` berarti waktu **mulai dari saat request diterima**, bukan penambahan menit. Untuk tambah waktu, backend menghitung sisa dari jadwal transaksi miliknya, lalu mengirim nilai baru. Durasi 1–86400 detik. Kendalikan drift/latensi dengan rekonsiliasi berkala; waktu tagihan tetap milik server. Backend yang mengetahui jam akhir sebaiknya menghitung ulang sisa ketika request belum pernah dikirim, lalu menyimpan payload untuk retry.

Stop:

```json
{"boot_id":"BOOT_ID","session_id":"rental-001","revision":3}
```

200=berhasil/idempotent, 400=payload tidak valid, 401=token salah, 409=konflik sesi/revision/boot/maintenance, 413=body terlalu besar. Duplicate identik tidak mengulang timer, termasuk setelah sesi habis. Revision lama ditolak. Sesi disimpan di RAM; restart selalu memulai dengan relay OFF. Backend membaca boot ID baru lalu memulihkan sesi sah dengan ID baru dan durasi tersisa. Relay status hanya status perintah GPIO, bukan pengukuran kontak/tegangan/layar TV.

Client admin sederhana (token melalui environment, tidak disimpan script):

```sh
export ESP_SWITCH_TOKEN='TOKEN_DARI_KONFIGURASI_LOKAL'
python3 scripts/client.py http://192.168.1.100 status
python3 scripts/client.py http://192.168.1.100 start --session rental-001 --revision 1 --seconds 3600
python3 scripts/client.py http://192.168.1.100 update --session rental-001 --revision 2 --seconds 1800
python3 scripts/client.py http://192.168.1.100 stop --session rental-001 --revision 3
```

Client contoh membaca status lebih dulu; backend produksi harus menyimpan payload/revision untuk retry, menangani 409, dan tidak otomatis memulihkan sesi yang sudah berakhir.

## OTA

1. Pastikan idle. Endpoint akan menolak saat sesi aktif.
2. Kirim `POST /maintenance/ota` dengan `{"boot_id":"BOOT_ID","enabled":true}` atau gunakan `scripts/client.py ... ota-open`.
3. Gunakan ArduinoOTA (`espota.py`) ke IP perangkat dengan password OTA lokal. Jendela terbuka 5 menit; sesi baru ditolak selama pemeliharaan. Untuk menutup sebelum waktunya, kirim `enabled:false`.
4. Setelah update/restart relay OFF. Baca ulang status dan verifikasi versi. Setiap firmware berikutnya harus tetap menyertakan fitur OTA.

Contoh dari PlatformIO setelah membuka jendela:

```sh
ESP_SWITCH_IP=192.168.1.100 ESP_SWITCH_OTA_PASSWORD='PASSWORD_LOKAL' python3 scripts/ota.py
```

Perangkat dan laptop perlu koneksi dua arah untuk ArduinoOTA. Transfer gagal tidak menjamin rollback otomatis; firmware rusak atau kehilangan Wi-Fi mungkin memerlukan pemulihan serial. Jangan memutus daya saat update. Firmware ini belum menerapkan OTA bertanda tangan, TLS, atau manajemen fleet.

## Verifikasi sebelum penggunaan rental

Firmware 0.1.1 menyediakan laporan serial diagnostik pada 57600 baud: scan startup menampilkan SSID, RSSI dalam dBm, channel, BSSID, kode enkripsi, dan status hidden (maksimal 50 access point). Setiap 10 detik board melaporkan status koneksi, IP, dan RSSI; saat belum tersambung, hasil scan startup diulang. Kirim karakter `S` melalui serial untuk membaca ulang laporan yang tersimpan. Laporan itu bukan scan baru. Scan hanya dilakukan saat startup dengan relay OFF agar tidak mengganggu timer sesi. Password/token tidak dicetak. Nama SSID hidden mungkin tetap kosong.

Tes logika native mencakup retry, revision lama, sesi habis, konflik sesi, batas durasi, dan rollover `millis()`. Selanjutnya uji pada meja tanpa AC: relay startup OFF, sesi pendek, tambah waktu, stop, Wi-Fi putus, reset, autentikasi, dan OTA idle. Fungsi server HTTP sinkron bisa menunda timer saat melayani client lambat; ketepatan cutoff di bawah beban jaringan masih harus diukur. Perilaku GPIO sebelum `setup()` belum dijamin oleh software.

Referensi: [ESP8266 Arduino OTA](https://arduino-esp8266.readthedocs.io/en/latest/ota_updates/readme.html).

## Manual relay override (0.1.2)

Semua request tetap memakai Bearer token. Baca `/status` untuk `boot_id` dan `generation`.

- `POST /relay/on`: `{"boot_id":"BOOT_ID","expected_generation":0,"request_id":"manual-001","duration_seconds":60}`. ON selama 1–86400 detik, lalu OFF otomatis. Ditolak selama sesi rental atau maintenance. Request ID unik; retry identik tidak mengulang timer.
- `POST /relay/off`: `{"boot_id":"BOOT_ID","expected_generation":1}`. OFF paksa dan membatalkan sesi manual/rental saat ini. Gunakan generation terbaru. Setelah membatalkan sesi aktif, generation naik sehingga retry lama ditolak (409); baca status untuk memastikan OFF.
- Status `mode`: `idle`, `rental`, atau `manual`. Tidak ada ON tanpa batas waktu. Restart tetap OFF. Rental API ditolak selama override manual aktif.

Endpoint ini memakai timer dan jalur GPIO yang sama dengan sesi rental; tidak melewati autentikasi atau interlock OTA.
