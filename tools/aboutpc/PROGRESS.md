# Progress & Roadmap

Status per modul. "Fase 1" = skeleton awal. "Fase 2" = implementasi
penuh untuk semua fitur yang direncanakan (proyek ini, belum
dikompilasi/diuji nyata). "Fase 3" = penyempurnaan lanjutan yang butuh
hasil pengujian nyata di 86Box terlebih dahulu.

## ✅ Fase 1 — Skeleton & Deteksi Dasar

- [x] Struktur proyek, `sysinfo.h`, enum status 5-kategori
- [x] Compatibility gate CPU: FLAGS-probing 8086 → 80186/188 → 80286 →
      80386 → 80486 → Pentium+ (CPUID)
- [x] CPUID vendor + brand string
- [x] RAM conventional (INT 12h) & extended (INT 15h E801h/88h)
- [x] Adapter video dasar (INT 10h AH=1Ah / klasifikasi mode)
- [x] Geometri & ukuran total HDD (INT 13h AH=08h)
- [x] Parsing MBR + tabel partisi primer
- [x] Scan signature SMBIOS ("_SM_")
- [x] Render module warna + `clear_screen()` portable + bar dekoratif

## ✅ Fase 2 — Implementasi Penuh (proyek ini, BELUM DIUJI)

- [x] **Clock speed CPU**:
      - CPU dgn CPUID + TSC (cek via CPUID leaf 1 EDX bit4): pakai
        **RDTSC**, diukur terhadap jendela waktu PIT (~0.5 detik) —
        akurat.
      - CPU tanpa TSC (8086-486, atau CPUID tapi tanpa TSC): fallback
        loop timing (DEC CX/JNZ, 65535 iterasi/blok) vs PIT tick,
        dikonversi ke MHz pakai tabel siklus/iterasi per generasi.
        **INI ESTIMASI, bukan pengukuran presisi** (+-10-20%
        diperkirakan) — tabel siklus/iterasi PERLU dikalibrasi ulang
        dengan membandingkan terhadap clock speed yang di-set manual
        di konfigurasi CPU 86Box.
- [x] **Nama drive fisik**: command **ATA IDENTIFY DEVICE** (0xECh)
      dikirim langsung ke port I/O controller IDE primary
      (0x1F0-0x1F7), asumsi drive BIOS 0x80 = primary master. Polling
      status pakai batas iterasi timeout (tidak akan hang tanpa
      batas). Kalau device lapor ERR, diasumsikan ATAPI (bukan HDD)
      dan ditandai N/A, bukan FAIL.
- [x] **Nama motherboard**: parsing tabel struct SMBIOS penuh — cari
      struct Type 2 (Baseboard Information), fallback ke Type 1
      (System Information) kalau Type 2 tak ada. Hanya bisa baca kalau
      alamat tabel struct < 1MB (lihat known limitation di bawah).
- [x] **Resolusi monitor**: VESA VBE DDC capability check (AX=4F15h
      BL=00h) → baca EDID block 0 (BL=01h) → validasi header + checksum
      → ekstrak H-active/V-active dari Detailed Timing Descriptor
      pertama (offset 54).
- [x] **Versi OS presisi**: ekstensi khusus kernel FreeDOS
      **INT 21h AX=33FFh** (return string versi kernel asli via far
      pointer DX:AX) — kalau tak didukung (bukan FreeDOS), fallback ke
      **INT 21h AH=30h** standar (versi DOS ter-emulasi + OEM ID).
- [x] Modul `detect_os.c/h` dipisah dari `main.c` untuk konsistensi
      struktur dengan modul detect_* lainnya.
- [x] Layout render digabung sesuai mockup asli: CPU+clock satu baris,
      RAM conventional+extended satu baris, disk name+size satu baris,
      monitor adapter+resolusi satu baris (via fungsi `merge_*_line()`
      di `main.c`).

## 🚧 Fase 3 — Belum Diimplementasi / Perlu Validasi Empiris

- [ ] **Kalibrasi tabel `cycles_per_iter_for_generation()`** di
      `detect_cpu.c` — angka saat ini dari dokumentasi timing Intel
      klasik, BELUM divalidasi. Rencana pengujian: set 86Box ke CPU
      spesifik (mis. 80286 @ 12 MHz), jalankan app, bandingkan hasil,
      sesuaikan konstanta kalau melenceng jauh (>20%).
- [ ] **Restore FLAGS lengkap** di `flags_probe()` (`detect_cpu.c`) —
      saat ini restore parsial di tahap AC-flag test, berpotensi
      meninggalkan AC flag ter-set setelah deteksi selesai. Perlu
      verifikasi di real 386+ / emulator apakah ini menyebabkan efek
      samping yang terlihat.
- [ ] **ATA IDENTIFY untuk slave/secondary controller** — saat ini
      cuma coba primary master (0x1F0). Kalau HDD ternyata di posisi
      lain (slave atau secondary controller 0x170), nama drive akan
      selalu N/A meski BIOS INT 13h berhasil baca drive-nya. Perlu
      logika deteksi controller/posisi yang lebih pintar (mis. coba
      berurutan: primary master → primary slave → secondary master →
      secondary slave, sampai salah satu merespons).
- [ ] **SMBIOS dengan tabel struct di atas 1MB** — field MACHINE akan
      selalu UNSUPPORTED di BIOS yang menaruh tabel struct-nya di atas
      1MB. Perlu unreal mode trick atau INT 15h AH=87h (move block)
      untuk baca dari real mode DOS. Penting dicek: apakah 86Box utk
      berbagai profil motherboard yang akan diuji menaruh tabelnya di
      bawah atau di atas 1MB (kemungkinan besar tergantung BIOS ROM
      yang diemulasikan per board).
- [ ] **VESA VBE fallback tanpa EDID** — kalau monitor/card tak
      dukung DDC, saat ini resolusi langsung UNSUPPORTED. Alternatif:
      baca mode video VESA yang sedang aktif (AX=4F03h - get current
      mode, lalu AX=4F01h - get mode info) untuk setidaknya tahu
      resolusi MODE TEKS/GRAFIS yang sedang dipakai, walau bukan
      resolusi native monitor sesungguhnya.
- [ ] Testing riil di Open Watcom + 86Box — **belum sama sekali**,
      ini akan jadi langkah pertama begitu source ini dianggap cukup
      matang untuk dicompile.

## Prinsip yang Dipegang (tidak berubah dari fase 1)

- Tidak ada modul yang boleh menyebabkan hang/crash — setiap BIOS/DOS
  interrupt call dan setiap port I/O langsung WAJIB dicek return
  code/timeout sebelum data dipakai. Semua loop polling (ATA, PIT)
  punya batas iterasi eksplisit.
- Field yang gagal dideteksi tidak pernah kosong begitu saja — selalu
  diisi teks status + kategori error yang sesuai.
- Kode yang melibatkan operasi berisiko (akses port I/O langsung,
  baca memori mentah BIOS/SMBIOS) ditulis dengan validasi checksum/
  signature/bounds-check di setiap langkah, tapi TETAP membutuhkan
  pengujian di hardware/emulator nyata sebelum dianggap benar-benar
  matang — dokumen ini mencatat dengan jujur bagian mana yang masih
  berupa estimasi/pendekatan (terutama clock speed CPU pra-Pentium).
