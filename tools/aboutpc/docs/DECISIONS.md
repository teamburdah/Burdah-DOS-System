# Catatan Keputusan Teknis

Ringkasan keputusan dari diskusi perencanaan, supaya tidak perlu
diulang alasannya tiap kali baca kode.

## 1. Kenapa satu file .EXE, bukan .EXE + .DLL?

DOS real-mode tidak punya mekanisme dynamic linking. DLL adalah
konsep Windows (format PE + loader Windows) yang tidak dikenal kernel
DOS sama sekali. Format executable native DOS cuma `.COM` (single
segment, maks 64KB) dan `.EXE` (format MZ, multi-segment). Untuk
aplikasi seukuran system-info tool ini, satu `.EXE` sudah cukup —
tidak perlu overlay (`.OVL`) yang biasanya dipakai untuk aplikasi besar
era DOS yang tak muat di memori sekaligus.

## 2. Kenapa Open Watcom, bukan Turbo C?

- Open Watcom mendukung target `-0` (instruksi set 8086 murni) secara
  eksplisit dan masih di-maintain untuk toolchain modern.
- Turbo C versi lama punya keterbatasan target CPU dan lebih merepotkan
  di-setup ulang di lingkungan development modern.
- Open Watcom inline assembler mendukung register 32-bit (`eax`, dst)
  bahkan dalam segmen 16-bit — dibutuhkan untuk uji AC flag/ID flag
  saat deteksi 386/486/CPUID.

## 3. Kenapa target minimum CPU 8086, bukan 286?

FreeDOS 1.4 masih resmi merilis **floppy edition** yang didesain
berjalan di IBM PC/XT/AT original (bukan cuma klaim dokumentasi).
Requirement resmi FreeDOS 1.4: CPU x86, BIOS, RAM 640 KB. Jadi 8086
adalah target kompatibilitas nyata, bukan skenario teoretis.

## 4. Kenapa kategori status 5 jenis (bukan cuma OK/FAIL)?

Supaya pesan ke user informatif soal *kenapa* data gagal:

| Status | Makna | Contoh |
|---|---|---|
| `OK` | Data berhasil dibaca | — |
| `N/A` | Fitur memang tidak ada di generasi hardware ini | CPU 286 tanpa CPUID |
| `UNSUPPORTED` | BIOS/hardware terlalu lama untuk fitur ini | BIOS pra-SMBIOS |
| `FAIL` | Command dikirim tapi gagal/timeout | ATA IDENTIFY timeout |
| `NOT_DETECTED` | Device sepertinya tidak ada | HDD tak terpasang |

Prinsip defensif: setiap BIOS/DOS interrupt call dicek carry flag /
return code SEBELUM data dipakai. DOS real-mode tidak punya exception
handling atau memory protection — general protection fault atau divide
error yang tak tertangani bisa hang/reboot sistem. Jadi mencegah lebih
baik daripada mengandalkan recovery yang memang tidak ada di DOS.

## 5. Kenapa clock speed & beberapa fitur awalnya ditandai TODO?

Di fase 1, timing loop kalibrasi clock speed, akses port I/O langsung
untuk ATA IDENTIFY, dan parsing SMBIOS lengkap sengaja ditunda karena
perlu diverifikasi di hardware/emulator sungguhan sebelum aman dianggap
benar. Di fase 2 (proyek ini), semuanya sudah diimplementasikan penuh
dengan penjelasan asumsi & keterbatasan yang jujur di komentar kode dan
`PROGRESS.md` — tapi TETAP belum dikompilasi/diuji nyata, jadi status
"matang secara desain" bukan "terverifikasi benar".

## 6. Kenapa clock speed pakai RDTSC untuk Pentium+ tapi loop-timing untuk yang lebih tua?

RDTSC (Read Time-Stamp Counter) adalah instruksi yang menghitung siklus
clock CPU sejak reset, presisi hingga per-siklus — tapi instruksi ini
baru ada di Pentium ke atas (dan hanya kalau CPU melaporkan dukungan
TSC lewat CPUID leaf 1). Untuk CPU 8086-486 yang tak punya RDTSC,
satu-satunya cara adalah loop timing manual: jalankan instruksi dengan
jumlah siklus per-iterasi yang (didekati) diketahui, ukur berapa lama
via PIT, lalu hitung mundur ke MHz. Ini secara inheren kurang presisi
karena siklus-per-instruksi bisa sedikit beda antar stepping/clone CPU
dalam generasi yang sama — makanya hasil loop-timing selalu diberi
label "estimasi" di UI, sementara hasil RDTSC ditampilkan sebagai angka
pasti.

## 7. Kenapa ATA IDENTIFY cuma coba primary master, tidak coba semua kombinasi?

Untuk menjaga scope fase 2 tetap terkelola dan supaya waktu deteksi
tetap singkat, kode saat ini berasumsi HDD pertama BIOS (drive 0x80)
ada di primary master (0x1F0) — asumsi yang benar di kebanyakan
konfigurasi standar termasuk default 86Box. Mencoba semua kombinasi
(primary/secondary × master/slave) itu lebih lengkap tapi menambah
kompleksitas dan waktu tunggu (tiap percobaan yang gagal butuh
menunggu timeout). Ditunda ke fase 3, terutama kalau pengujian di
86Box (dengan konfigurasi board yang berbeda-beda) menunjukkan ini
perlu.

## 8. Kenapa nama field OS pakai INT 21h AX=33FFh, bukan cuma AH=30h?

AH=30h (fungsi standar sejak DOS 2.0) melaporkan versi DOS yang
DI-EMULASIKAN untuk kompatibilitas software — bisa dipengaruhi utility
SETVER dan biasanya dilaporkan sebagai versi MS-DOS generik (mis.
7.10) meski kernel yang sebenarnya berjalan adalah FreeDOS. AX=33FFh
adalah ekstensi khusus yang ditambahkan pengembang kernel FreeDOS untuk
melaporkan versi KERNEL ASLI (mis. build 2045) — jauh lebih informatif
untuk tujuan "System Properties" ini, dan sekaligus jadi cara paling
diandalkan untuk memastikan yang berjalan memang FreeDOS, bukan sekadar
DOS lain yang kompatibel. Fallback ke AH=30h tetap dipertahankan untuk
kasus non-FreeDOS.
