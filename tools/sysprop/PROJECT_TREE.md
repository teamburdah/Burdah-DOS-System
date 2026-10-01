# Struktur Proyek

```
dos-sysprop/
├── README.md              Overview proyek, target kompatibilitas, cara build
├── PROGRESS.md             Status implementasi per modul + roadmap fase 3
├── PROJECT_TREE.md         File ini
├── makefile.wat            Build script untuk Open Watcom (wmake)
├── docs/
│   └── DECISIONS.md        Catatan keputusan teknis dari diskusi
├── src/
│   ├── sysinfo.h            Struct data bersama + enum status_t
│   │                        (dipakai SEMUA modul, header paling penting)
│   │
│   ├── detect_os.h/.c       Versi OS: ekstensi kernel FreeDOS
│   │                        (INT 21h AX=33FFh) + fallback standar
│   │                        (INT 21h AH=30h)
│   │
│   ├── detect_cpu.h/.c      Compatibility gate: tahap 1 (8086 vs 286+,
│   │                        kuirk FLAGS bit 12-15, instruksi 16-bit
│   │                        murni) + tahap 2 (286 vs 386+, VIA COBA
│   │                        EKSEKUSI pushfd dilindungi setjmp/longjmp +
│   │                        handler INT 6 -- BUKAN lagi kuirk FLAGS,
│   │                        lihat DECISIONS.md poin 10 kenapa diganti),
│   │                        fallback clock speed loop-timing vs PIT
│   │
│   ├── detect_cpu_x86.h/.c  Compatibility gate tahap 3&4 (386 vs 486 vs
│   │                        Pentium+/CPUID) + CPUID vendor/brand string
│   │                        + tabel Family/Model (nama CPU lebih
│   │                        spesifik saat tanpa brand string) + RDTSC
│   │                        + probe_pushfd_raw (dipakai detect_cpu.c
│   │                        lewat setjmp/longjmp). WAJIB dikompilasi
│   │                        target CPU tinggi (-5) -- lihat
│   │                        DECISIONS.md poin 9(a) & 10
│   │
│   ├── detect_mem.h/.c      RAM conventional (INT 12h) & extended
│   │                        (INT 15h E801h/88h)
│   │
│   ├── detect_disk.h/.c     Geometri & ukuran HDD (INT 13h AH=08h),
│   │                        parsing MBR + tabel partisi, NAMA DRIVE via
│   │                        ATA IDENTIFY DEVICE (akses port I/O langsung
│   │                        ke controller IDE primary)
│   │
│   ├── detect_board.h/.c    Scan + PARSING PENUH tabel struct SMBIOS
│   │                        (Type 2 Baseboard, fallback Type 1 System)
│   │                        untuk nama motherboard
│   │
│   ├── detect_video.h/.c    Adapter video (MDA/CGA/EGA/VGA) via INT 10h,
│   │                        RESOLUSI via VESA VBE DDC + parsing EDID
│   │
│   ├── render.h/.c          Output teks berwarna via BIOS teletype
│   │                        (INT 10h AH=09h/0Eh), clear_screen(),
│   │                        bar dekoratif 15-warna
│   │
│   └── main.c               Orkestrasi: cls → panggil semua detect_*()
│                            → gabung field terkait (merge_*_line) →
│                            render ke layar sesuai layout System Properties
│
└── build/                   sysprop.exe hasil kompilasi TERVERIFIKASI
                             (Open Watcom + DOSBox) ada di sini, siap
                             dicoba di 86Box
```

## Status Build (Fase 3.5)

Source ini SUDAH berhasil dikompilasi & dijalankan nyata (Open Watcom
2.0 beta + DOSBox 0.74-3), bukan cuma ditulis berdasar dokumentasi.
`build/sysprop.exe` adalah hasil build dari source ASLI di folder ini
(termasuk `AX=33FFh` yang aktif, belum divalidasi di FreeDOS
sungguhan -- lihat PROGRESS.md fase 4). Compile ulang manual:

```
export WATCOM=/path/ke/ow-snapshot
export PATH="$WATCOM/binl64:$PATH"   # atau binl utk 32-bit Linux
export INCLUDE="$WATCOM/h"
cd src
wcc -0 -ms -zq -oh -d0 detect_cpu.c     -fo=../build/detect_cpu.obj
wcc -5 -ms -zq -oh -d0 detect_cpu_x86.c -fo=../build/detect_cpu_x86.obj
wcc -0 -ms -zq -oh -d0 main.c detect_os.c detect_mem.c detect_disk.c detect_board.c detect_video.c render.c -fo=../build/
cd ../build
wlink system dos name sysprop file { main.obj detect_os.obj detect_cpu.obj detect_cpu_x86.obj detect_mem.obj detect_disk.obj detect_board.obj detect_video.obj render.obj }
```

**Catatan penting**: `detect_cpu_x86.c` WAJIB target `-5`, file lain
WAJIB target `-0` -- lihat DECISIONS.md poin 9(a). `makefile.wat` yang
disediakan SUDAH disesuaikan dan **TERVERIFIKASI jalan dengan `wmake`**
(diuji nyata: `wmake -f ../makefile.wat` dari folder `src/` berhasil
menghasilkan `sysprop.exe` yang identik dengan build manual).

## Alur Dependensi Modul

```
sysinfo.h  ←──┬── detect_os.c     (INT 21h)
              ├── detect_cpu.c    (FLAGS/CPUID/RDTSC/PIT)
              ├── detect_mem.c    (INT 12h/15h)
              ├── detect_disk.c   (INT 13h + port I/O ATA)
              ├── detect_board.c  (scan ROM + parsing SMBIOS)
              ├── detect_video.c  (INT 10h + VESA/EDID)
              └── render.c        (INT 10h teletype)
                     │
                     ▼
                  main.c   (satu-satunya yang tahu urutan render & layout,
                            termasuk logika gabung field lintas-modul)
```

Setiap `detect_*.c` **tidak saling bergantung satu sama lain** — modular,
bisa dikembangkan/diuji terpisah. `render.c` juga tidak tahu apa-apa
soal cara data didapat, cuma tahu cara menampilkan `field_t` dengan
warna sesuai `status_t`. `main.c` adalah satu-satunya tempat yang
menyatukan urutan tampilan sesuai layout yang disepakati, termasuk
menggabungkan field yang secara data terpisah (mis. nama CPU + clock
speed) jadi satu baris tampilan.

## Modul dengan Akses Hardware Langsung (Fase 2)

Berbeda dari fase 1 yang seluruhnya lewat BIOS/DOS interrupt, fase 2
menambahkan dua modul yang mengakses hardware LEBIH LANGSUNG:

- `detect_disk.c` — port I/O langsung ke controller IDE (0x1F0-0x1F7)
  untuk command ATA IDENTIFY DEVICE.
- `detect_board.c` — baca memori ROM mentah (F000:0000+) untuk scan &
  parsing tabel SMBIOS, bukan lewat fungsi BIOS resmi (SMBIOS memang
  didesain untuk dibaca langsung dari memori, bukan lewat interrupt).

Kedua modul ini punya validasi berlapis (timeout, checksum, bounds-
check) karena tidak ada BIOS yang "menjaga" kalau terjadi kesalahan —
beda dengan modul lain yang tinggal mengandalkan carry flag dari BIOS
interrupt.

## Output Build

Satu file `sysprop.exe` (format MZ, real-mode, tanpa dependency DLL/
overlay — lihat `docs/DECISIONS.md` untuk alasan teknisnya).
