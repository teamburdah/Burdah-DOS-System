# Catatan Modifikasi: FreeDOS → Burdah-DOS System 0.5 (Bahasa Indonesia)

Dokumen ini mencatat seluruh proses remaster kernel FreeDOS menjadi
**Burdah-DOS System 0.5**, mencakup alur kerja, keputusan teknis, dan
**seluruh bug yang ditemukan selama proses** — dicatat apa adanya, termasuk
yang sempat terlihat seperti bug besar tapi ternyata bukan.

Target hardware utama: **Dell System 200 (Intel 80286)**, diuji lewat
**PCem** (akurasi hardware) dan **QEMU** (iterasi cepat).

---

## 1. Ringkasan Toolchain

| Komponen | Alat |
|---|---|
| Compiler kernel | Open Watcom 2 (build Linux, `owlinux`) |
| Assembler | NASM |
| Kompresi kernel | UPX |
| Shell / `COMMAND.COM` | FreeCOM (`github.com/FDOS/freecom`), Open Watcom |
| Pembuatan image floppy | `mtools` (`mformat`, `mcopy`, `mattrib`) |
| Uji boot | QEMU (`qemu-system-i386`), PCem (validasi akhir oleh pengguna) |

---

## 2. Alur Kerja Kronologis

### 2.1 Audit awal & percobaan build pertama

Source Burdah-DOS 0.5 di-upload sebagai arsip zip. Audit pertama:
mengecek apakah string `"FreeDOS"` sudah diganti `"Burdah"` di seluruh
source.

**Temuan**: rebranding string identitas (banner boot, pesan bootloader,
`SYS.COM`, versi kernel) **sudah lengkap dan konsisten**. Sisa 60 file
yang masih menyebut "FreeDOS" semuanya di komentar lisensi, nama fungsi
internal (`FreeDOSmain()`), atau dokumentasi — tidak memengaruhi identitas
runtime.

Percobaan compile pertama **gagal total**. Ini bug pertama yang ditemukan.

### 2.2 Command "COLOR" (setelah build pertama sukses)

Setelah image floppy pertama berhasil di-boot, permintaan pertama adalah
menambahkan command internal `COLOR bf` (gaya CMD Windows) ke `COMMAND.COM`
(FreeCOM).

Langkah:
1. `cmd/color.c` dibuat — pakai BIOS `INT 10h AH=06h` (scroll/clear window)
   untuk mengecat ulang layar dengan attribute baru, meniru pola yang
   sudah dipakai `cmd/cls.c`.
2. Didaftarkan ke `shell/cmdtable.c`, `include/command.h`, `config.h`
   (`INCLUDE_CMD_COLOR`), `cmd/makefile.mak`.
3. Teks bantuan (`HELP COLOR`) ditambahkan ke `strings/DEFAULT.lng` —
   sistem string FreeCOM sudah punya infrastruktur multi-bahasa,
   jadi teks bantuan otomatis ikut sistem lokalisasi yang ada.
4. Diuji lewat boot QEMU: perubahan attribute BIOS terverifikasi (warna
   biru/kuning sesuai `COLOR 1E`), `errorlevel` diverifikasi benar untuk
   kasus sukses dan gagal (`COLOR zz` → error, errorlevel 1).

### 2.3 Diskusi arsitektur `XCPU=86` vs kompatibilitas PC modern

Sesi refleksi soal kenapa kernel di-assemble dengan `XCPU=86` (baseline
8086) alih-alih target CPU lebih baru. Kesimpulan: `XCPU=86` adalah
superset kompatibilitas penuh (jalan di 8086 sampai CPU x86-64 modern
sekalipun, karena mode real x86 tetap backward-compatible), sedangkan
`XCPU=386` justru **menutup pintu** ke hardware 286 yang jadi target utama
proyek ini. Ditemukan juga mekanisme keamanan bawaan: kernel yang
di-compile untuk CPU tertentu (`XCPU != 86`) otomatis punya kode
pengecekan CPU saat boot (`cpu_abort`) yang menolak berjalan dengan pesan
error yang jelas, bukan crash.

### 2.4 Audit konsistensi versi "0.5"

Audit menyeluruh string versi di seluruh source:

- Banner `BURDAH-DOS 0.5` **konsisten** di 7 file inti yang sudah memilikinya.
- **Belum konsisten**: mayoritas file kernel lain (`task.c`, `dosfns.c`,
  `fatfs.c`, dll.) belum punya header Burdah sama sekali (bukan salah
  versi, tapi belum tersentuh).
- `hdr/version.h` → macro `KERNEL_VERSION` masih placeholder `"- GIT "`,
  bukan `"0.5"` — inilah yang benar-benar tercetak di layar boot
  (`Burdah System - GIT (build ...)`).
- `docs/fdkernel.lsm` (metadata paket) masih `Version: git`.
- `COMMAND.COM` (FreeCOM) melapor versi sendiri `0.87` — desain yang wajar
  (komponen terpisah, versioning independen), bukan bug, tapi perlu
  keputusan sadar apakah mau diseragamkan.

> **Status**: perbaikan untuk poin-poin di atas (`KERNEL_VERSION` →
> `"0.5"`, header Burdah di semua file, update `fdkernel.lsm`) **sempat
> diusulkan tapi belum dieksekusi** — percakapan berbelok ke topik OEM ID
> dan HELP command sebelum ada keputusan. Ini **pekerjaan tertunda**, lihat
> §5.

### 2.5 Identitas machine-readable: masalah OEM ID

Pengguna menjalankan `dosfetch` (tool neofetch-style untuk DOS,
`github.com/leahneukirchen/dosfetch`) di boot image Burdah, hasilnya:

```
OS: FreeDOS 7.10
```

Meski seluruh teks banner sudah "Burdah", `dosfetch` tetap mendeteksi
FreeDOS. Investigasi menemukan akar masalahnya: identitas DOS untuk
software pihak ketiga **tidak** dibaca dari teks banner, melainkan dari
**INT 21h AH=30h** ("Get DOS Version"), register `BH` = *OEM number* —
1 byte konvensi industri yang mengidentifikasi vendor DOS. Kernel Burdah
masih memakai `OEM_ID = 0xFD`, yaitu **ID resmi yang dipakai FreeDOS
sendiri**.

**Perbaikan**:
1. `hdr/version.h`: `OEM_ID` diganti dari `0xFD` → **`0xBB`** (identitas
   unik Burdah, dicek tidak bentrok dengan ID vendor lain yang diketahui:
   `0x00` IBM, `0xFF` Microsoft generik, `0xFD` FreeDOS, `0x05` Zenith,
   `0x23` Olivetti).
2. `kernel/kernel.asm`: `Version_OemID` disinkronkan ke `0xBB` (nilai ini
   di-duplikasi manual di dua tempat sesuai konvensi proyek yang sudah
   ada — ada komentar eksplisit "must be kept in sync with VERSION.H").
3. **OEM ID dibuat *runtime-configurable*, bukan cuma konstanta
   compile-time** — field baru `oem_id` ditambahkan ke struct LoL
   (`hdr/lol.h`, disinkronkan ke layout data `kernel.asm`), dan
   `kernel/inthndlr.c` diubah membaca variabel ini alih-alih macro tetap.
4. Directive baru `OEMID=` ditambahkan ke `CONFIG.SYS` (`kernel/config.c`,
   mengikuti pola `VERSION=` yang sudah ada) — memungkinkan sistem
   "menyamar" sebagai FreeDOS (`OEMID=0xFD`) atau ID lain saat boot, tanpa
   perlu compile ulang, untuk berjaga-jaga ada software lama yang
   bermasalah.
5. Diuji lewat program kecil `TESTOEM.EXE` yang langsung memanggil
   `INT 21h AH=30h` dan mencetak `BH` — dikonfirmasi berubah dari `0xFD`
   ke `0xBB` (default) dan bisa disetel ulang ke `0xFD` lewat
   `CONFIG.SYS`.

Setelah pengguna mem-patch `dosfetch` miliknya sendiri untuk mengenali
`0xBB`, hasilnya berubah jadi `Burdah DOS System 7.10` — mengonfirmasi
seluruh rantai perbaikan bekerja.

### 2.6 Command "HELP" — TUI scrollable

Dengan 53 command internal terdaftar, daftar command tidak mungkin
ditampilkan sekaligus di layar 80×25 (DOS text mode tidak punya
scrollback). Pengguna menyiapkan mockup UI (title bar cyan, tabel
No/Command/Purpose, instruksi navigasi, tanpa scrollbar untuk hemat
resource) dan memandu implementasinya.

Desain akhir:
- Layout tetap 25 baris: title (1), instruksi (2–3), spacer (4),
  border atas tabel (5), header (6), border header (7), viewport data
  scrollable (8–24, 17 baris), border penutup (25, muncul hanya saat
  scroll mencapai command terakhir).
- Navigasi: panah UP/DOWN untuk scroll, ESC atau `X`/`x` untuk keluar
  (dibaca lewat `cgetchar()` + `include/keys.h`, infrastruktur yang
  sudah ada di FreeCOM).
- Kolom **Purpose** diambil otomatis dari baris pertama teks
  `TEXT_CMDHELP_*` tiap command (lewat `getString()`) — tidak perlu tabel
  deskripsi terpisah yang harus dirawat manual; command baru otomatis
  muncul di HELP begitu ditambahkan.
- Digambar pakai BIOS `INT 10h` murni (tanpa akses memori video
  langsung), mengikuti konvensi seluruh codebase FreeCOM yang sudah ada.

---

## 3. Daftar Bug yang Ditemukan (Transparan)

### 3.1 Bug nyata & sudah diperbaiki

| # | Bug | Penyebab | Perbaikan |
|---|---|---|---|
| 1 | Build kernel gagal total | `git submodule` `country/` dan `share/` kosong di arsip zip (bukan hasil `git clone --recursive`) | Clone manual dari `github.com/FDOS/country` dan `github.com/FDOS/share` |
| 2 | Build gagal di tahap kompresi kernel | Binary `upx`/`upx-ucl` belum terpasang di environment | `apt-get install upx-ucl` |
| 3 | `fixstrs.exe` (tool build string resource FreeCOM) **segfault** saat menambah command `HELP` | `#define MAXSTRINGS 256` di `strings/fixstrs.c` — array tetap, overflow saat total string resource mencapai 257 | `MAXSTRINGS` dinaikkan ke 512 |
| 4 | Layar HELP terus bergeser ke atas walau kontennya "diam" | Menulis karakter ke sel pojok kanan-bawah layar (baris 25 kolom 80) lewat BIOS **teletype** (`INT 10h AH=0Eh`) memicu auto-scroll seluruh layar — kebetulan posisi ini persis di sudut kanan-bawah border penutup HELP | Semua penulisan karakter di `help.c` diganti dari teletype ke `INT 10h AH=09h` (tulis langsung di posisi, tidak pernah memicu scroll) |
| 5 | OEM ID kernel masih `0xFD` (ID resmi FreeDOS) | Warisan langsung dari source FreeDOS asli, tidak ikut ter-audit di sesi rebranding string pertama karena bukan teks yang tampak di layar | Diganti `0xBB` + dibuat runtime-configurable lewat `OEMID=` di `CONFIG.SYS` |

### 3.2 Bug yang TERNYATA bukan bug (dicatat untuk transparansi proses)

Selama debugging border penutup HELP (baris 25), sempat muncul gejala
yang terlihat seperti bug kedua yang terpisah dari bug auto-scroll di
atas: border hanya tampil **sebagian** atau **tidak tampil sama sekali**
di baris tertentu. Proses isolasi:

1. Diuji ulang berkali-kali dengan mengirim 40–60 keystroke `DOWN` secara
   beruntun-cepat lewat QEMU monitor (`sendkey`) — jauh lebih cepat dari
   kecepatan mengetik manusia — lalu screenshot diambil segera sesudahnya.
2. Hasil tampak "stabil" (sama di beberapa kali screenshot berturut-turut)
   sehingga awalnya disangka bug nyata, bukan sekadar frame terpotong.
3. Isolasi sistematis (menguji tiap karakter box-drawing satu per satu:
   `BX_BL`, `BX_BT`, `BX_BR`, lalu kombinasi ketiganya) — **semua lolos**
   saat diuji dengan jeda waktu yang wajar.
4. Kesimpulan akhir: ini **artefak metodologi testing**, bukan bug kode.
   DOS punya buffer keyboard terbatas (~15 entri); mengirim puluhan
   keystroke jauh lebih cepat dari kecepatan redraw (setiap redraw penuh
   ≈1400 pemanggilan BIOS `INT 10h`) menyebabkan screenshot menangkap
   *frame robek* (`torn frame`) — potongan render yang belum tuntas —
   bukan representasi state yang sesungguhnya.
5. Setelah diuji ulang dengan pola tekan-tombol realistis (satu per satu,
   jeda ≈0.4–0.5 detik meniru manusia), border penutup terbukti render
   sempurna di setiap kondisi, termasuk tepat setelah command terakhir
   (`53 WHICH`) di baris 24.

**Pelajaran**: dicatat di sini secara terbuka karena beberapa giliran
percakapan dihabiskan mengejar bug yang sebenarnya tidak ada — supaya
jelas prosesnya, bukan disembunyikan.

---

## 4. Daftar File yang Dimodifikasi

### Kernel (`kernel/`, `hdr/`)

| File | Perubahan |
|---|---|
| `hdr/version.h` | `OEM_ID`: `0xFD` → `0xBB` |
| `hdr/lol.h` | Tambah field `oem_id` di struct LoL |
| `kernel/kernel.asm` | `Version_OemID`: `0xFD`→`0xBB`; tambah field runtime `_oem_id` di data LoL |
| `kernel/globals.h` | Tambah `extern ASM oem_id` |
| `kernel/inthndlr.c` | Handler `INT 21h AH=30h` baca `oem_id` (variabel) alih-alih `OEM_ID` (macro tetap) |
| `kernel/config.c` | Directive baru `OEMID=` (`CfgOemId()`, mengikuti pola `sysVersion()`) |

### Shell / `COMMAND.COM` (`freecom/`)

| File | Perubahan |
|---|---|
| `cmd/color.c` | **Baru** — command `COLOR bf` |
| `cmd/help.c` | **Baru** — command `HELP`, TUI tabel command scrollable |
| `include/command.h` | Prototype `cmd_color()`, `cmd_help()` |
| `shell/cmdtable.c` | Registrasi `COLOR`, `HELP` di tabel command internal |
| `config.h` | `INCLUDE_CMD_COLOR`, `INCLUDE_CMD_HELP` |
| `cmd/makefile.mak` | Tambah `color.obj`, `help.obj` ke daftar objek |
| `strings/DEFAULT.lng` | Teks bantuan `TEXT_CMDHELP_COLOR`, `TEXT_CMDHELP_HELP` |
| `strings/fixstrs.c` | `MAXSTRINGS`: 256 → 512 (perbaikan bug build) |

### Alat bantu uji (bukan bagian sistem, disertakan di floppy untuk verifikasi)

- `TESTOEM.EXE` — utilitas kecil pemanggil `INT 21h AH=30h`, mencetak versi DOS
  dan OEM ID (`BH`), untuk verifikasi cepat tanpa perlu `dosfetch`.

---

## 5. Pekerjaan Tertunda / Belum Dieksekusi

Item berikut **sudah dibahas dan disetujui arahnya** tapi **belum
diimplementasikan** — dicatat di sini supaya tidak hilang dari radar:

1. `KERNEL_VERSION` di `hdr/version.h` masih `"- GIT "`, belum diganti
   `"0.5"` — ini yang menentukan apa yang tercetak di banner boot
   (`Burdah System - GIT (...)` seharusnya jadi `Burdah System 0.5 (...)`).
2. Header banner `BURDAH-DOS 0.5` belum ditambahkan ke mayoritas file
   kernel (`task.c`, `dosfns.c`, `fatfs.c`, dll.) — konsistensi kosmetik,
   tidak memengaruhi fungsi.
3. `docs/fdkernel.lsm` masih `Version: git`, belum `0.5`.
4. Keputusan soal versi `COMMAND.COM` (`FreeCom version 0.87`) — apakah
   dibiarkan independen atau diseragamkan ke skema versi Burdah.
5. `README.md` dan dokumentasi lain di `docs/` masih sepenuhnya bahasa
   FreeDOS asli, belum disentuh sama sekali.
6. Redraw `HELP` cukup berat di sisi BIOS (\~1400 panggilan `INT 10h` per
   redraw penuh) — belum jadi masalah fungsional, tapi berpotensi terasa
   agak lambat di hardware 286 sungguhan saat scroll cepat. Kandidat
   optimasi kalau perlu.

---

## 6. Cara Build Ulang (Ringkasan)

```bash
# 1. Kernel
export WATCOM=/path/to/open-watcom
export PATH=$PATH:$WATCOM/binl64
make all COMPILER=owlinux        # hasil: bin/kernel.sys, bin/kwc8632.sys, dll.

# 2. FreeCOM (COMMAND.COM)
cd freecom
./build.sh watcom                # hasil: command.com

# 3. Floppy image 1.44MB
mformat -i burdah.img -f 1440 -v BURDAH ::
# tulis boot sector (boot/fat12com.bin, menjaga BPB dari mformat)
mcopy -i burdah.img -o BURDAH.SYS COMMAND.COM COUNTRY.SYS CONFIG.SYS AUTOEXEC.BAT SYS.COM ::
mattrib -i burdah.img +s +h ::BURDAH.SYS ::COMMAND.COM
```

Detail lengkap tiap langkah (termasuk kenapa submodule harus di-clone
manual) ada di §2 dan §3 dokumen ini.
