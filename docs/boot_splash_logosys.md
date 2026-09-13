# Rencana B — Boot Splash ala Win9x (Level Kernel)

> **STATUS: SUDAH DIEKSEKUSI** (diimplementasikan pengguna, diaudit dan
> di-build oleh Claude pada sesi 27–28 Agustus 2026). Dokumen ini
> dipertahankan sebagai catatan sejarah/rencana awal — untuk detail
> implementasi, bug yang ditemukan, dan hasil akhir yang sebenarnya,
> lihat `docs/BURDAH-DOS-MODIFICATION-LOG.md` §3.1 (bug #11, #12) dan §4
> (subseksi "Boot Splash — Rencana B"). Sebagian detail arsitektur di
> bawah ini ternyata sesuai perkiraan; lainnya (misalnya kebutuhan poke
> register CRTC manual untuk 320×400) ternyata tidak diperlukan karena
> implementasi akhir memakai mode 13h standar (320×200), bukan trik
> 320×400 asli Win9x.

Status lama (sebelum dieksekusi): Disimpan sebagai rencana untuk versi Burdah DOS
setelah 0.5. Rencana A (splash via BBAUTO.BAT, level userspace) dikerjakan
lebih dulu sebagai uji coba/batu loncatan sebelum masuk ke rencana ini.

## Latar Belakang

MS-DOS 7.x (Windows 95/98/ME) menampilkan `C:\LOGO.SYS` langsung dari
`IO.SYS`, sebelum `CONFIG.SYS` diproses. Ini bukan fitur shell/COMMAND.COM,
melainkan logika bawaan kernel DOS itu sendiri. Referensi lengkap
perbandingan pendekatan ada di riwayat percakapan sesi 15 Agustus 2026
(build `burdahdos.img` pertama).

## Yang Dibutuhkan

| Komponen | Status di kernel Burdah (Agustus 2026) | Kesulitan |
|---|---|---|
| Reader gambar sebelum CONFIG.SYS diproses | Kernel sudah punya FAT driver sendiri (`fatfs.c`/`fatdir.c`), jadi baca file di titik ini secara teknis dimungkinkan | Sedang |
| Mode grafis 320×400 asli (ala LOGO.SYS asli) | Kernel murni text-mode (INT 10h AH=0Eh/09h saja) — nol infrastruktur grafis | **Tinggi** — perlu poke register CRTC/DAC langsung, bukan lewat INT 10h biasa |
| Parser bitmap (raw 8bpp + palet, atau BMP asli) | Tidak ada | Sedang, ringan kalau format raw tanpa header BMP penuh |
| Menyembunyikan pesan `DEVICE=`/init saat splash aktif | Kernel saat ini mencetak semua status device satu-satu (`config.c`, `initdisk.c`, `initclk.c`, dll) | **Tinggi** — banyak titik print perlu dibungkus kondisional |
| Sinkronisasi splash selesai vs CONFIG.SYS selesai + skip (ESC) | Belum ada | Sedang |

## Titik Sentuh Kernel (perkiraan awal)

- `kernel/main.c` — fungsi `signon()` (titik yang sama dengan fix komentar
  bersarang di sesi build pertama), untuk memicu splash sebelum proses
  CONFIG.SYS dimulai.
- `kernel/config.c` — sumber sebagian besar pesan `DEVICE=` yang perlu
  dibungkus kondisional "jangan cetak kalau splash aktif".
- `kernel/initdisk.c`, `kernel/initclk.c` — sumber pesan status device
  lain yang juga tercetak saat init.
- File baru untuk logic mode grafis + blit (belum ada nama modul).

## Resiko yang Perlu Diantisipasi

1. **Perbedaan perilaku VGA antar-emulator.** Trik 320×400 (unchained +
   page-flip via register CRTC/sequencer non-standar) tidak dijamin
   berperilaku identik di PCem (emulasi 286 + card period-akurat) vs QEMU
   (biasanya Cirrus/std VGA). Wajib diuji di kedua emulator, idealnya juga
   di hardware 286 asli sebelum dianggap selesai.
2. **Resiko regresi pada jalur boot paling sensitif.** Modifikasi di titik
   paling awal kernel (sebelum device apa pun siap) sulit didebug karena
   belum ada output layar untuk melacak titik gagal jika terjadi hang.
   Kernel yang sekarang (0.5) sudah terbukti boot bersih dan terverifikasi
   — perubahan di area ini harus dikerjakan hati-hati dan bertahap.
3. **Cakupan perubahan lebih luas** dibanding modifikasi kernel sebelumnya
   (yang hanya menyentuh satu file `main.c`/`version.h`) — beberapa file
   kernel tersentuh sekaligus, butuh siklus test-boot berulang.

## Opsi Kompromi (kalau 320×400 asli dianggap terlalu beresiko)

Gunakan mode 13h standar (320×200) saja lewat INT 10h biasa, tanpa poke
register manual. Mengorbankan sedikit "keotentikan" tinggi gambar
dibanding LOGO.SYS asli, tapi jauh lebih aman lintas-emulator karena
seluruhnya lewat panggilan BIOS standar — sama seperti rencana A, hanya
dipindah lebih awal ke dalam kernel dan ditambah logic sembunyikan pesan
init.

## Prasyarat Sebelum Mulai

- Rencana A (splash BBAUTO.BAT) sudah selesai, teruji, dan gambar/palet
  final sudah disetujui secara visual.
- Format data gambar (raw 8bpp + palet 768 byte) dari rencana A bisa
  dipakai ulang di sini — tidak perlu dirancang dari nol.
