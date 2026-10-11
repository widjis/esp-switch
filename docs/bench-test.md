# BA129 bench test — 11 Oktober 2026

Firmware 0.1.1; ESP8266EX MAC `84:F3:EB:5E:0C:3A`; flash 4 MB. Flash serial melalui bridge C3 pada 57600 baud, USB RX buffer 16 KB. esptool 5.3.1 berhasil menulis firmware dan memverifikasi hash; uploader esptool 3 bawaan PlatformIO gagal mengirim stub.

Scan langsung dari board menemukan SSID `MTI-01` pada 2,4 GHz. Nama awal `mti-01` berbeda huruf besar/kecil, menghasilkan kegagalan koneksi. Konfigurasi lokal dikoreksi; password/token tidak dimasukkan Git. IP saat pengujian `10.60.21.190`, RSSI terhubung sekitar −58 sampai −61 dBm. IP DHCP bisa berubah; gunakan reservasi pada MikroTik.

## Hasil yang terverifikasi

- HTTP status terautentikasi berhasil; permintaan tanpa token mendapat 401.
- Status awal relay OFF.
- Start menghasilkan status relay ON.
- Retry payload identik tidak mengulang timer.
- Update mengganti sisa waktu.
- Perintah revision lama ditolak dengan 409.
- Timer 4 detik setelah update berakhir; status relay menjadi OFF tanpa perintah stop.
- Sesi kedua bisa dimulai dan dihentikan secara eksplisit.
- Pembukaan OTA saat sesi aktif ditolak.
- OTA saat idle terbuka; start sesi saat maintenance ditolak.
- Upload OTA binary yang sama berhasil pada percobaan kedua. Percobaan pertama terputus saat transfer; penyebab belum terisolasi.
- Sesudah OTA, boot ID berubah, endpoint status kembali tersedia, relay OFF, sesi kosong, dan jendela OTA tertutup. Versi tetap 0.1.1 karena binary pengujian sama.

## Batas bukti

Relay ON/OFF di atas adalah status firmware/GPIO, bukan pengukuran kontak atau daya splitter. Belum ada konfirmasi pemutusan gambar TV, uji jaringan putus, relay glitch sebelum setup, beban client lambat, atau penerimaan fisik pada instalasi rental. OTA sekali berhasil setelah satu kegagalan bukan bukti reliabilitas jangka panjang.
