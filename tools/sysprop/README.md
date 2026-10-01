# dos-sysprop

Utilitas "System Properties" sederhana untuk FreeDOS/MS-DOS 16-bit,
terinspirasi tampilan ala `fastfetch` tapi minimalis (tanpa ASCII art
logo). Menampilkan info dasar CPU, RAM, board, disk, dan video dalam
format teks berwarna, langsung di command line DOS.

## Target Kompatibilitas

- **OS**: FreeDOS 1.4 (prioritas utama), kompatibel juga dengan MS-DOS
  versi lama dan varian DOS lain yang mengikuti standar INT 21h/INT 13h.
- **CPU minimum**: Intel 8086/8088 (real mode murni, tanpa instruksi
  386+). Ini sejalan dengan FreeDOS 1.4 yang masih merilis edisi floppy
  untuk mesin IBM PC/XT/AT asli.
- **CPU maksimum**: mendukung deteksi hingga CPU dengan CPUID (Pentium
  generation ke atas) untuk nama prosesor detail.
- **Memori**: berjalan di 640 KB conventional memory (tanpa perlu extended
  memory driver seperti HIMEM.SYS).

## Fitur (lihat status detail di `PROGRESS.md`)

| Info | Metode Deteksi | Status |
|---|---|---|
| Nama & generasi CPU | FLAGS-register probing (8086→Pentium) + CPUID | Fase 1 (jalan) |
| Clock speed CPU | Timing loop vs PIT | Fase 2 (belum) |
| RAM conventional/extended | INT 12h, INT 15h (E801h/88h) | Fase 1 (jalan) |
| Nama motherboard | Scan SMBIOS di ROM F000:0000 | Fase 2 (parsial) |
| Nama & ukuran harddisk | INT 13h (geometri), MBR (partisi) | Fase 1 (parsial) |
| Model drive fisik (ATA IDENTIFY) | Akses port I/O langsung | Fase 2 (belum) |
| Adapter video | INT 10h (deteksi mode & VGA flag) | Fase 1 (jalan) |
| Resolusi monitor (VESA/EDID) | INT 10h AX=4F15h | Fase 2 (belum) |
| Versi DOS (OEM) | INT 21h AX=3306h | Fase 2 (belum) |

Setiap field punya salah satu dari 5 status: `OK`, `N/A` (fitur memang
tak ada di hardware ini), `UNSUPPORTED` (BIOS/hardware terlalu lama),
`FAIL` (command dikirim tapi gagal), `NOT_DETECTED` (device tak ada).
Field yang gagal **tidak pernah** membuat aplikasi force-close — selalu
fallback ke pesan status yang sesuai.

## Skema Warna

- Label field (`CPU`, `RAM`, dst): abu-abu terang (default, attribute `07h`)
- Data valid: hijau neon (attribute `0Ah`)
- Data error/N/A: merah terang (attribute `0Ch`)
- Bar dekoratif bawah: siklus 15 warna foreground standar teks DOS, blok
  penuh CP437 219 (`0xDB` / `█`)

## Toolchain

- **Compiler**: Open Watcom C/C++ 16-bit
- **Target**: `-0` (instruksi set 8086 murni), memory model Small (`-ms`)
- **Linker**: `wlink`, target `system dos`
- **Build**: `wmake -f makefile.wat` (lihat `PROJECT_TREE.md` untuk lokasi)

> Kode di repo ini ditulis dan direview manual, **belum dikompilasi**
> di lingkungan sandbox (tidak ada toolchain DOS cross-compiler
> tersedia). Rekomendasi pengujian: compile dengan Open Watcom asli,
> lalu jalankan di emulator seperti **86Box** atau **DOSBox-X**
> (DOSBox biasa tidak mengemulasikan CPU 8086 murni dengan akurat
> untuk uji compatibility gate).

## Cara Build (setelah install Open Watcom)

```
cd src
wmake -f ../makefile.wat
```

Menghasilkan `sysprop.exe` — file tunggal, tanpa DLL/overlay (lihat
alasan teknis di `docs/DECISIONS.md`).

## Struktur Proyek

Lihat `PROJECT_TREE.md`.

## Progres & Roadmap

Lihat `PROGRESS.md`.
