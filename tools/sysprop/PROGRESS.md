# Progress & Roadmap

Status per modul. "Fase 1" = skeleton awal. "Fase 2" = implementasi
penuh semua fitur. "Fase 3" = penyempurnaan lanjutan (SMBIOS >1MB,
ATA multi-posisi, restore FLAGS, fallback VESA). "Fase 3.5" = build &
uji nyata (Open Watcom + DOSBox), 4 bug nyata ditemukan+diperbaiki.
**"Fase 4" (baru) = perbaikan gerbang deteksi 286-vs-386+ yang
TERNYATA tidak andal di lingkungan virtualisasi (86Box, QEMU) +
tabel Family/Model CPUID untuk nama CPU lebih spesifik.**

## 🚨 Fase 4 — Perbaikan Kritis: Gerbang Deteksi 286-vs-386+

**Bug yang dilaporkan user**: diuji lintas beberapa profil mesin
86Box + QEMU, CPU HAMPIR SELALU terdeteksi generik "Intel 80286 atau
kompatibel" walau CPU sebenarnya jauh lebih baru (estimasi clock
speed di QEMU sampai ~1GHz, tapi nama tetap "80286"). Diagnosis: BUKAN
cuma kekurangan database nama -- gerbang deteksi generasi CPU (tahap
2: 286 vs 386+) salah simpul karena tes lama (kuirk FLAGS bit IOPL/NT
via POPF 16-bit) TIDAK ANDAL di banyak lingkungan virtualisasi
(hypervisor diduga memvirtualisasi/membatasi manipulasi IOPL demi
keamanan). Akibatnya cabang CPUID/RDTSC TIDAK PERNAH tercapai, apapun
CPU sebenarnya.

- [x] **Tahap 2 diganti total**: dari "tes kuirk nilai FLAGS" menjadi
      "coba EKSEKUSI instruksi 386+ sungguhan (pushfd), tangkap fault
      dgn aman via `setjmp`/`longjmp` + handler INT 6". Jauh lebih
      andal krn menguji kapabilitas eksekusi instruksi NYATA -- sesuatu
      yg wajib diemulasikan benar oleh emulator/hypervisor manapun.
- [x] **Tabel Family/Model CPUID** (`cpu_model_table[]` di
      `detect_cpu_x86.c`) -- nama CPU lebih spesifik saat brand string
      tak tersedia (Pentium klasik s.d. awal PII/Celeron, + AMD K5-K7
      awal). Kombinasi tak dikenali tetap ditampilkan apa adanya +
      status NA, tak pernah mengarang nama.
- [x] **Diverifikasi di DOSBox**: SEBELUM perbaikan, app SELALU
      melaporkan "80286" bahkan di DOSBox (yg CPU virtualnya jelas
      386+ ke atas). SETELAH perbaikan, app berhasil mencapai cabang
      CPUID, melaporkan "GenuineIntel Family 4 Model 0" (identitas
      CPUID sintetis DOSBox). Program tetap selesai tanpa hang.
- [ ] **BELUM diverifikasi**: jalur FAULT (longjmp dipanggil krn CPU
      sungguhan menolak pushfd) -- DOSBox tak bisa emulasikan 286
      asli, jadi baru "jalur sukses" yg teruji jalan. **INI PALING
      PENTING divalidasi di 86Box dgn profil mesin 286 ASLI** sebelum
      dianggap 100% aman -- lihat DECISIONS.md poin 10 utk detail
      lengkap risikonya.
- [ ] 8086 vs 8088 -- BELUM diimplementasi, butuh teknik timing bus
      8-bit vs 16-bit yg tidak 100% andal lintas clone chip, sengaja
      ditunda drpd memberi kepastian palsu.
- [ ] Nama spesifik 286/386SX vs DX/486SX vs DX (tanpa CPUID) --
      BELUM diimplementasi, kandidat fase berikutnya (via tes FPU
      internal, pola serupa handler INT 6 di atas tapi utk INT 7).

## ✅ Status Build: BERHASIL DIKOMPILASI & DIUJI JALAN

- `sysprop.exe` berhasil dibuild dari source asli di folder ini
  (16.274 byte, format MZ DOS valid) memakai Open Watcom C 2.0 beta,
  target CPU `-0` (8086) untuk mayoritas modul, `-5` (Pentium) khusus
  untuk `detect_cpu_x86.c`.
- Diuji jalan di DOSBox 0.74-3 (headless via Xvfb + screenshot):
  program tampil rapi, berwarna sesuai spesifikasi (hijau=OK,
  merah=N/A/error), selesai tanpa hang, kembali ke prompt normal.
- **PENTING**: pengujian DOSBox di atas dilakukan dengan `AX=33FFh`
  (deteksi versi kernel FreeDOS) DI-STUB sementara khusus untuk sesi
  uji tsb, karena DOSBox pakai kernel DOS internal minimalis (bukan
  FreeDOS sungguhan) yang tidak mendukung fungsi ini dan abort saat
  dipanggil. **File `sysprop.exe` yang diserahkan ke kamu dibuild dari
  SOURCE ASLI (`AX=33FFh` aktif, TIDAK di-stub)** -- ini perlu
  divalidasi di 86Box + FreeDOS 1.4 sungguhan, target uji yang benar.
  Lihat DECISIONS.md poin 9(e) untuk detail lengkap.

## 🐛 Bug Nyata Ditemukan & Diperbaiki Lewat Proses Build+Test

Lihat DECISIONS.md poin 9 untuk detail teknis lengkap tiap poin:

1. **Assembler Watcom menolak instruksi 386+/CPUID/RDTSC saat target
   compile `-0`** -- bukan cuma soal optimisasi/runtime, tapi validasi
   compile-time yang keras. **Perbaikan struktural**: `detect_cpu.c`
   dipecah jadi 2 file -- `detect_cpu.c` (8086-safe, target `-0`) dan
   `detect_cpu_x86.c` (butuh 386+/CPUID/RDTSC, target `-5`). File baru
   `detect_cpu_x86.h` mendokumentasikan kontrak "kapan aman dipanggil".
2. **`seg`/`off` sbg nama variabel bentrok dgn operator SEG/OFFSET**
   Watcom -- error compile jelas, diganti `r_seg`/`r_off` di
   `detect_os.c`.
3. **`ch` sbg nama parameter bentrok dgn register CH** -- compile
   SUKSES tapi diam-diam salah (baca register, bukan parameter),
   membuat output layar berantakan. Diperbaiki di `render.c`
   (parameter diganti `c`). Baru ketahuan dari HASIL VISUAL nyata di
   DOSBox, bukan dari membaca kode.
4. **`[nama_pointer]` di `_asm` TIDAK men-dereference pointer** --
   membaca alamat variabel pointer itu sendiri, bukan data yang
   ditunjuknya. Ditemukan di `put_string_plain()` (`render.c`),
   menyebabkan separator " : " tercetak sbg byte sampah "./0" alih-
   alih string yang benar. Diperbaiki dgn dereference di C biasa dulu
   (`unsigned char c = (unsigned char)*s;`) baru dipakai di `_asm`.
5. **`MK_FP`/`FP_SEG`/`FP_OFF` ada di `<i86.h>`, bukan `<dos.h>`** --
   ditambahkan include yg benar di `detect_os.c` & `detect_board.c`.

## ✅ Fase 1 — Skeleton & Deteksi Dasar

- [x] Struktur proyek, `sysinfo.h`, enum status 5-kategori
- [x] Compatibility gate CPU dasar (8086→286→386+), CPUID vendor+brand
- [x] RAM conventional/extended, adapter video dasar, geometri HDD,
      parsing MBR, scan signature SMBIOS, render module + bar warna

## ✅ Fase 2 — Implementasi Penuh Semua Fitur

- [x] Clock speed (RDTSC utk CPU+TSC, fallback loop-timing vs PIT)
- [x] Nama drive fisik via ATA IDENTIFY DEVICE (port I/O langsung)
- [x] Nama motherboard via parsing tabel SMBIOS penuh (Type 2/Type 1)
- [x] Resolusi monitor via VESA VBE DDC + parsing EDID
- [x] Versi OS presisi via ekstensi kernel FreeDOS (INT 21h AX=33FFh)

## ✅ Fase 3 — Penyempurnaan Lanjutan

- [x] Restore EFLAGS 32-bit penuh di `flags_probe`/`flags_probe_extended`
      (bug restore-parsial dari fase 2 diperbaiki)
- [x] ATA IDENTIFY coba 4 posisi (primary/secondary x master/slave)
- [x] SMBIOS di atas 1MB via BIOS move-block (`INT 15h AH=87h`)
- [x] Fallback resolusi monitor tanpa EDID (baca mode VESA aktif)

## ✅ Fase 3.5 — Build & Uji Nyata (BARU)

- [x] Toolchain Open Watcom terpasang & berfungsi
- [x] Semua 9 modul berhasil dikompilasi tanpa error
- [x] Link berhasil menghasilkan `sysprop.exe` (format MZ valid)
- [x] Dijalankan nyata di DOSBox, terverifikasi tampil benar & selesai
      tanpa hang (dengan catatan stub AX=33FFh khusus sesi uji, lihat
      di atas)
- [x] 4 bug nyata ditemukan & diperbaiki (lihat daftar di atas)

## 🚧 Fase 5 — Belum Diimplementasi / Perlu Validasi di Target Asli

- [ ] **[PALING KRITIS] Validasi jalur FAULT di `cpu_supports_386_instructions()`
      pada 286 ASLI di 86Box** -- lihat DECISIONS.md poin 10. DOSBox
      cuma bisa memvalidasi "jalur sukses" (CPU mendukung pushfd);
      jalur longjmp/INT6-catch BELUM PERNAH benar-benar teruji jalan
      krn tak ada cara reproduksi CPU 286 asli di sandbox ini. Kalau
      app hang/crash spesifik di mesin 286 86Box, curigai ini duluan.
- [ ] **Validasi `AX=33FFh` di FreeDOS 1.4 sungguhan** (86Box) -- ini
      PENTING sebelum dianggap benar-benar selesai, karena DOSBox
      tidak bisa memvalidasi jalur kode ini (lihat di atas).
- [ ] **Kalibrasi tabel `cycles_per_iter_for_generation()`** di
      `detect_cpu.c` -- BELUM dibandingkan dgn clock speed asli yg
      di-set di konfigurasi 86Box -- masih perlu validasi silang,
      SEKARANG lebih relevan krn gerbang deteksi generasi sudah benar
      (fase 4), jadi konstanta per-generasi ini akan lebih sering
      terpakai & lebih penting akurat.
- [ ] **Uji dgn CPU generasi lain** (386/486/Pentium ASLI, bukan cuma
      identitas sintetis DOSBox) di 86Box utk pastikan jalur RDTSC &
      CPUID vendor/brand string bekerja dgn data CPU sungguhan.
- [ ] **Perluas `cpu_model_table[]`** kalau ditemukan CPU CPUID-capable
      dgn Family/Model yg belum ada di tabel saat uji di 86Box (lihat
      DECISIONS.md poin 11) -- tabel sengaja tak lengkap, mudah
      ditambah entry baru.
- [ ] **Uji dgn HDD asli ter-mount di 86Box** (bukan DOSBox `mount c:`
      yg tidak menyediakan device IDE nyata) -- supaya ATA IDENTIFY,
      geometri INT13h, dan parsing MBR/partisi bisa tervalidasi
      dgn data asli, bukan semua N/A seperti di sesi DOSBox kemarin.
- [ ] **Uji dgn berbagai profil motherboard 86Box** -- lihat mana yang
      punya SMBIOS <1MB, >1MB, atau tidak sama sekali; MACHINE field
      di sesi DOSBox kemarin selalu N/A (wajar, DOSBox tak sediakan
      SMBIOS sama sekali).
- [ ] Deteksi 8086 vs 8088, dan nama spesifik 286/386SX vs DX/486SX
      vs DX tanpa CPUID -- lihat DECISIONS.md poin 11, kandidat fase
      berikutnya, perlu kehati-hatian sama spt fase 4.
- [ ] Restore FLAGS di `flags_probe_basic` (tahap 1, 8086/286) masih
      pakai 16-bit popf sederhana -- ini SUDAH benar utk CPU generasi
      itu (FLAGS mereka memang cuma 16-bit), tidak perlu diubah, tapi
      dicatat di sini utk kejelasan.

## Prinsip yang Dipegang (diperkuat oleh fase 3.5)

- Tidak ada modul yang boleh menyebabkan hang/crash -- semua interrupt
  call & port I/O dicek return code/timeout, semua loop polling ada
  batas iterasi. TERBUKTI di uji DOSBox: program selalu selesai &
  kembali ke prompt, termasuk saat 4 percobaan ATA IDENTIFY semuanya
  gagal (total 800.000 iterasi timeout) -- tidak hang.
- Field yang gagal dideteksi tidak pernah kosong -- selalu diisi teks
  status + kategori error yang sesuai. TERKONFIRMASI visual di
  screenshot uji: N/A merah, data valid hijau, sesuai spesifikasi.
- **BARU (fase 3.5): kode assembly/hardware-level HARUS dikompilasi &
  dijalankan nyata sebelum dianggap benar** -- 4 dari 5 bug yang
  ditemukan (poin b-d) TIDAK akan ketahuan dari review kode manual
  sekomprehensif apapun, karena gejalanya diam-diam salah (bukan error
  compile), dan baru terlihat dari POLA ANEH di output visual nyata.
