# Catatan Modifikasi: FreeDOS → Burdah-DOS System 0.5

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
| 6 | `portal/shell/ver.c` gagal compile total (`FREECOM_VERSION` tidak terdefinisi) | Macro didefinisikan sebagai `FreeCOM_VERSION` di `version.h` (huruf kecil-besar campur), tapi dipakai sebagai `FREECOM_VERSION` (huruf besar semua) di `ver.c` — C bersifat case-sensitive | Macro diseragamkan jadi `FREECOM_VERSION`; sekaligus blok deteksi nama compiler (`__WATCOMC__` dkk.) dihapus dari `shellver[]` atas permintaan eksplisit (konsistensi string versi) |
| 7 | `kernel/globals.h` gagal compile (`Expecting ';' but found '('`) | `KVS`/`xKVS` di `hdr/version.h` direbrand jadi string literal polos (`"Burdah DOS System\n"`), padahal dipanggil sebagai macro fungsi 3-argumen (`xKVS(KERNEL_VERSION, REVISION_SEQ, OEM_ID)`) di `globals.h` | `KVS`/`xKVS` dikembalikan jadi macro fungsi yang benar, dengan stringifikasi argumen numerik (`REVISION_SEQ`, `OEM_ID`), tetap memuat teks "Burdah DOS System" |
| 8 | `kernel/main.c` gagal compile (W300 nested comment, warning-as-error) | Komentar blok deteksi compiler ditutup salah: `/* ... /` (kurang satu `*`) alih-alih `*/`, membuat Watcom mendeteksi komentar bersarang | Blok komentar mati (sisa deteksi nama compiler, sudah ditandai "tidak diperlukan di Burdah" oleh komentar aslinya) dihapus seluruhnya |
| 9 | `sys/fdkrncfg.c` menampilkan `"DOS-C"`, bukan `"Burdah"`, untuk kernel ber-OEM ID `0xBB` | Kondisi `displayConfigSettings()` cuma cek `Version_OemID == 0xFD`, jatuh ke fallback `"DOS-C"` untuk ID lain — patch pengenalan `0xBB` yang pernah dibahas belum masuk ke source ini | Ditambah kondisi `(Version_OemID == 0xBB) ? "Burdah" : ...` sebelum fallback `0xFD`/`DOS-C` |
| 10 | `portal/suppl/src/nls/english.err` dan `portal/strings/DEFAULT.err`/`english.err` hilang total dari arsip source, build Portal berhenti di tahap SUPPL dan STRINGS | Generator asli (`mkerrfct.pl`) juga tidak ada di source ini; kemungkinan file-file ini tidak ikut ter-commit di titik sebelumnya | `nls/english.err` dibuat sebagai placeholder kosong (diverifikasi: semua macro `E_*` yang dipakai sudah lengkap di `english.lng`); `DEFAULT.err` direkonstruksi dari format resmi `critstrs.c` (tabel pesan critical-error handler DOS standar); `english.err` dibuat dummy kosong (pola sama seperti `english.lng`) |
| 11 | Boot splash (`LOGO.SYS`) tidak pernah tampil — `splash_init()` selalu gagal diam-diam | Kandidat file pertama yang dicoba `open()` adalah `<BootDrive>:\BURDAH.SYS` — nama itu **sama persis** dengan file kernel itu sendiri, yang pasti selalu ada di disk. `open()` langsung sukses di percobaan pertama, lalu validasi signature BMP gagal (karena isinya kernel binary, bukan BMP) dan fungsi langsung `return 0` — kode tidak pernah lanjut mencoba `LOGO.SYS` sama sekali | Direstrukturisasi jadi loop 4 kandidat (`<BootDrive>:\LOGO.SYS`, `<BootDrive>:\BURDAH.SYS`, `\LOGO.SYS`, `\BURDAH.SYS` — `LOGO.SYS` diprioritaskan) yang lanjut ke kandidat berikutnya kalau validasi header BMP gagal, bukan cuma kalau `open()` gagal |
| 12 | **[FATAL]** Kernel hang permanen (freeze total, tidak merespons apa pun) tepat setelah boot splash tampil, setiap kali splash aktif | Di `kernel/console.asm`, fungsi `_int29_handler`: instruksi `cmp byte [cs:_splash_active], 0` + `jne .int29hndlr_ret` diletakkan **sebelum** blok `push ax/si/di/bp/bx`, padahal label `.int29hndlr_ret` langsung melakukan 5× `pop` tanpa syarat. Begitu splash aktif dan karakter pertama dicetak ke layar (baris `"Press F8 to trace..."` di `config.c`, dipanggil lewat `ConWrite`→`INT 29h`), handler langsung lompat ke `.int29hndlr_ret` **tanpa pernah push**, sehingga 5× `pop` itu menarik 10 byte dari stack milik pemanggil (bukan nilai yang seharusnya) — merusak stack lalu `iret` ke alamat acak. Dilacak dengan teknik debug marker: tulis byte penanda langsung ke VRAM (`pokeb`) di titik-titik checkpoint sepanjang `main.c`→`config.c`→`SkipLine()`, dibaca lewat `screendump` QEMU — supaya tidak terpengaruh suppression teks yang justru sedang didiagnosis | Blok `push` dipindah ke **awal** handler (sebelum pengecekan `_splash_active`), sehingga baik jalur "splash aktif, lewati" maupun jalur "cetak karakter" sama-sama menuju `.int29hndlr_ret` dengan stack yang seimbang |

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

### 3.3 Teknik: debug marker VRAM langsung (untuk bug #12)

Dicatat karena tekniknya kemungkinan berguna lagi untuk debugging kernel
level-boot di masa depan, terutama untuk apa pun yang terjadi *sebelum*
atau *selagi* jalur output teks normal tidak bisa diandalkan (persis
situasi bug #12: splash sedang aktif men-*suppress* semua teks).

Prinsipnya: tulis byte warna secara langsung ke framebuffer VGA (`pokeb`
ke segmen `0xA000`) di titik-titik checkpoint sepanjang alur kode yang
dicurigai, lalu baca hasilnya lewat `screendump` QEMU (via monitor socket)
setelah proses berhenti/hang — tanpa pernah melewati `printf`/BIOS
teletype/`INT 29h` sama sekali, sehingga hasil debug tidak mungkin ikut
"termakan" oleh bug suppression yang sedang diselidiki.

Langkah yang dipakai:
1. Satu baris pixel (row tetap, mis. baris 90) dibersihkan dulu ke warna
   solid gelap (index palet yang di-override paksa ke hitam) sebagai
   "kanvas bersih" — supaya marker putih di baris itu tidak ambigu dengan
   warna asli gambar splash (percobaan pertama tanpa langkah ini sempat
   menghasilkan positif-palsu karena background gambar sudah putih di
   sebagian titik).
2. Fungsi kecil `splash_mark(x)` dipanggil di puluhan titik checkpoint
   berurutan (spasi kolom cukup lebar antar checkpoint supaya tidak
   tumpang tindih), dari `main.c` (level fungsi) turun ke `config.c`
   (level per-baris `CONFIG.SYS`) sampai ke dalam `SkipLine()` (level
   pra/pasca setiap pemanggilan `printf`/`GetBiosKey`).
3. Proses dijalankan di QEMU headless (`-display none`), dibiarkan
   berjalan/hang, lalu satu `screendump` diambil dan dibaca pixel-per-pixel
   dengan Python/PIL untuk menentukan checkpoint mana yang sempat
   ter-tulis (dicapai) dan mana yang tidak (titik hang persis berada
   tepat setelah checkpoint terakhir yang menyala).
4. Setelah bug ditemukan dan diperbaiki, **semua** instrumentasi ini
   (fungsi `splash_mark`/`splash_clear_track` beserta seluruh
   pemanggilannya) dihapus total dari source sebelum build rilis final —
   tidak ada sisa kode debug di `burdahdos.img` yang didistribusikan.

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

### Sesi Build Pertama ke `burdahdos.img` (15 Agustus 2026)

| File | Perubahan |
|---|---|
| `portal/version.h` | Macro `FreeCOM_VERSION` → `FREECOM_VERSION` (perbaikan bug #6) |
| `portal/shell/ver.c` | Hapus blok deteksi nama compiler dari `shellver[]` |
| `hdr/version.h` | `KVS`/`xKVS` dikembalikan jadi macro fungsi yang benar (perbaikan bug #7) |
| `kernel/main.c` | Hapus blok komentar mati yang rusak nesting-nya (perbaikan bug #8) |
| `sys/fdkrncfg.c` | Tambah pengenalan OEM ID `0xBB` → `"Burdah"` (perbaikan bug #9) |
| `portal/suppl/src/nls/english.err` | **Baru** — placeholder (perbaikan bug #10) |
| `portal/strings/DEFAULT.err` | **Baru** — tabel pesan critical-error handler (perbaikan bug #10) |
| `portal/strings/english.err` | **Baru** — dummy override kosong (perbaikan bug #10) |
| `bin/kernel.sys` → `bin/BURDAH.SYS` | Rename hasil build kernel sesuai nama yang di-hardcode di `sys/fdkrncfg.c` |

Hasil: `burdahdos.img` (1.44MB) berhasil dirakit dan **terverifikasi boot**
di QEMU (screenshot boot + `ver` command) dan dikonfirmasi lancar oleh
pengguna di PCem.

### Boot Splash — Rencana A (uji coba, 15 Agustus 2026)

| File | Perubahan |
|---|---|
| `tools/splash/splash.asm` | **Baru** — utilitas `SPLASH.COM`. Mode 13h (320×200, 256 warna) murni lewat `INT 10h`/`INT 16h`/`INT 1Ah` (tanpa akses register VGA langsung, supaya aman lintas 286 asli/PCem/QEMU). Alur: set mode → load palet → blit pixel data (di-*embed* via `incbin`) → tunggu keypress **atau** timeout ±5 detik (91 tick BIOS) → kembali ke mode teks → keluar |
| `tools/splash/logo.raw` | **Baru** — data pixel mentah 320×200, 1 byte/pixel (index palet), 64.000 byte. **Placeholder**, belum logo final |
| `tools/splash/logo.pal` | **Baru** — palet 256 warna VGA DAC (6-bit/komponen), 768 byte. **Placeholder** |
| `bin/SPLASH.COM` | **Baru** — hasil build `splash.asm`, disertakan di floppy image |
| `bin/bbauto.bat` | Tambah pemanggilan `SPLASH.COM` di baris pertama |

Diuji 3 skenario di QEMU (screenshot terlampir di setiap sesi terkait):
splash tampil benar (warna & teks sesuai preview), timeout otomatis
kembali ke mode teks, lanjut boot normal ke prompt `A:\>`. Jalur skip
manual (tombol ditekan sebelum timeout) **belum diuji eksplisit** —
logic-nya ada (`INT 16h AH=01h` polling) tapi belum diverifikasi lewat
simulasi keystroke.

Rencana lanjutan (splash level kernel, sebelum `CONFIG.SYS`, ala Win9x
`LOGO.SYS` asli) didokumentasikan terpisah di
`docs/ROADMAP-BOOT-SPLASH.md` — **belum dikerjakan**, menunggu logo final
dan hasil evaluasi Rencana A ini.

### Boot Splash — Rencana B (level kernel, 27–28 Agustus 2026)

Rencana B diimplementasikan sendiri oleh pengguna di sesi kerja terpisah
(bukan oleh Claude), lalu diserahkan untuk audit, perbaikan bug, dan
build rilis final. Implementasi memakai `docs/logo.bmp` (320×200, 8bpp,
uncompressed BI_RGB — persis format yang divalidasi parser) sebagai
sumber gambar asli, bukan lagi placeholder Rencana A.

| File | Perubahan |
|---|---|
| `kernel/initsplsh.c` | **Baru** (dibuat pengguna). Parser BMP manual + blit langsung ke `0xA000:0000` dalam mode 13h. Diaudit dan diperbaiki (bug #11, lihat §3.1) |
| `kernel/init-mod.h` | Deklarasi `splash_active`, `splash_init()`, `splash_check_abort()`, `splash_close()` |
| `kernel/console.asm` | `_int29_handler` di-hook untuk suppress semua output teks selagi `splash_active=1` (lewat `INT 29h`, dipakai `ConWrite`). **Diperbaiki bug fatal #12** (lihat §3.1) — urutan `push`/pengecekan `_splash_active` yang salah menyebabkan stack corruption dan hang total |
| `kernel/main.c` | `splash_init()` dipanggil sebelum `DoConfig(0)`; `splash_close()` dipanggil setelah `InitializeAllBPBs()`, sebelum shell dijalankan — membungkus seluruh proses `CONFIG.SYS` |
| `kernel/config.c` | `splash_check_abort()` dipanggil per-baris (polling `ESC`/keypress apa pun untuk skip manual); `splash_close()` dipanggil di titik-titik yang butuh tampilan interaktif (prompt Y/N, F5/F8 trace) supaya splash tidak menutupi prompt yang sedang menunggu respons |
| `kernel/makefile` | `initsplsh.obj` ditambahkan ke `OBJS7` dan aturan build-nya |
| `docs/logo.bmp` | **Baru** (disediakan pengguna) — logo final 320×200, 8bpp |
| `bin/LOGO.SYS` | Disertakan di floppy image (salinan `docs/logo.bmp`, di-*rename*) |
| `bin/bbauto.bat` | Baris pemanggilan `SPLASH.COM` (Rencana A) **dihapus** — splash sekarang ditangani level kernel, lebih awal dari `bbauto.bat`; memanggil keduanya akan menampilkan dua splash berurutan |

**Metodologi audit sebelum build:** source diperiksa dulu tanpa langsung
build — memverifikasi `initsplsh.obj` sudah terdaftar di makefile,
menelusuri seluruh call-site `splash_active`/`splash_init`/`splash_close`
lintas file, dan memvalidasi `docs/logo.bmp` byte-per-byte terhadap
format yang diharapkan parser (lebar/tinggi/bitcount/compression) sebelum
percobaan build pertama.

**Proses debugging bug #12 (fatal):** build pertama sukses tanpa error
compile, tapi boot hang total setelah splash tampil. Diisolasi bertahap:
(1) dikonfirmasi hang hilang total kalau `LOGO.SYS` tidak ada di image
(splash tidak pernah aktif) — memastikan penyebabnya terkait splash,
bukan `CONFIG.SYS`; (2) dikonfirmasi hang tetap terjadi walau semua
baris `DEVICE=` yang device driver-nya hilang (`udvd2.sys`) dihapus dari
`CONFIG.SYS` uji — menyingkirkan dugaan awal soal device driver hilang;
(3) debug marker VRAM (lihat §3.3) dipasang bertahap dari level fungsi
turun ke level pra/pasca-`printf` untuk mempersempit sampai ke baris
persis; (4) root cause ditemukan di `console.asm`, diperbaiki, diverifikasi
ulang dengan marker yang sama menunjukkan seluruh alur `DoConfig()`
tuntas sampai `splash_close()`; (5) seluruh instrumentasi debug dihapus,
kernel di-build ulang bersih untuk rilis final.

Hasil akhir: `burdahdos.img` (1.44MB) dengan Rencana B aktif, **boot
splash `LOGO.SYS` tampil sejak detik pertama** (sebelum pesan
`CONFIG.SYS` apa pun), lanjut normal ke shell Portal 1.05 tanpa hang,
diverifikasi lewat screenshot QEMU (splash + `ver` command) dengan
`CONFIG.SYS` **asli** pengguna (termasuk baris `DEVICE=A:\udvd2.sys`
yang filenya memang tidak disertakan di floppy — device gagal load
seperti yang diharapkan, tapi boot tetap tuntas, tidak hang).

---

## 5. Pekerjaan Tertunda / Belum Dieksekusi

Item berikut **sudah dibahas dan disetujui arahnya** tapi **belum
diimplementasikan** — dicatat di sini supaya tidak hilang dari radar:

1. ~~`KERNEL_VERSION` di `hdr/version.h` masih `"- GIT "`~~ — **selesai**,
   sudah `"0.5 "` per sesi build 15 Agustus 2026, terverifikasi tercetak
   benar di banner boot (`Burdah DOS System version 0.5 (build 46, OEM:0xbb)`).
2. Header banner `BURDAH-DOS 0.5` belum ditambahkan ke mayoritas file
   kernel (`task.c`, `dosfns.c`, `fatfs.c`, dll.) — konsistensi kosmetik,
   tidak memengaruhi fungsi.
3. `docs/fdkernel.lsm` masih `Version: git`, belum `0.5`.
4. Keputusan soal versi `COMMAND.COM` (`FreeCom version 0.87`) — apakah
   dibiarkan independen atau diseragamkan ke skema versi Burdah. **Catatan:**
   nama shell sendiri sudah beres (`Portal version 1.05`, lihat §4 sesi
   build pertama) — item ini spesifik soal string versi `FreeCom` lama
   kalau masih ada sisa di tempat lain yang belum teraudit.
5. `README.md` dan dokumentasi lain di `docs/` masih sepenuhnya bahasa
   FreeDOS asli, belum disentuh sama sekali. Termasuk `portal/README.md`
   yang masih 100% teks asli "FreeCom - The DOS Command Line Interface".
6. Redraw `HELP` cukup berat di sisi BIOS (\~1400 panggilan `INT 10h` per
   redraw penuh) — belum jadi masalah fungsional, tapi berpotensi terasa
   agak lambat di hardware 286 sungguhan saat scroll cepat. Kandidat
   optimasi kalau perlu.
7. `nls/english.err`, `DEFAULT.err`, `english.err` (lihat §3.1 bug #10)
   dibuat sebagai placeholder/rekonstruksi manual karena generator asli
   (`mkerrfct.pl`) tidak ada di source ini. Kalau generator itu pernah
   ditemukan lagi, file-file ini sebaiknya di-generate ulang dari situ,
   bukan dipertahankan manual selamanya.
8. ~~Boot splash Rencana A (`SPLASH.COM`) masih pakai gambar placeholder~~
   — **digantikan**, bukan lagi dilanjutkan. Rencana B (level kernel)
   sudah aktif dan dipakai di rilis final; pemanggilan `SPLASH.COM` sudah
   dihapus dari `bin/bbauto.bat`. File `tools/splash/*` masih ada di
   source sebagai referensi/uji coba tapi tidak lagi dipakai di image.
9. ~~Boot splash Rencana B (level kernel, ala `LOGO.SYS` Win9x asli)~~ —
   **selesai**, diimplementasikan pengguna lalu diaudit/diperbaiki/di-build
   per sesi 27–28 Agustus 2026 (lihat §3.1 bug #11, #12 dan §4). Splash
   `LOGO.SYS` tampil sejak detik pertama boot, terverifikasi tidak hang.
10. File biner leftover yang perlu dibersihkan sebelum rilis: `bin/kwc8632.sys`,
    `bin/kwc8632.map` (duplikat kernel dari proses build, nama lama),
    `portal/strings/fixstrs.exe` (build artifact yang ikut ter-commit).
11. Jalur skip manual splash Rencana B (tombol ditekan sebelum
    `CONFIG.SYS` selesai diproses) belum diuji eksplisit lewat simulasi
    keystroke — logic-nya ada (`splash_check_abort()` dipanggil per-baris
    CONFIG.SYS, polling `INT 16h AH=01h`) tapi verifikasi yang dilakukan
    sejauh ini hanya jalur non-interaktif (splash otomatis tertutup di
    akhir `DoConfig()`).
12. `tools/splash/` (Rencana A: `splash.asm`, `logo.raw`, `logo.pal`,
    `splash.com`) sekarang tidak terpakai di alur boot — kandidat untuk
    dihapus dari source sepenuhnya di rilis berikutnya kalau memang tidak
    akan dipakai lagi, atau dipertahankan sebagai cadangan/referensi.
13. `docs/ROADMAP-BOOT-SPLASH.md` isinya sekarang sudah usang (ditulis
    sebagai rencana sebelum Rencana B benar-benar dikerjakan) — perlu
    diperbarui atau ditandai "sudah dieksekusi, lihat
    BURDAH-DOS-MODIFICATION-LOG.md §4" supaya tidak membingungkan pembaca
    di masa depan.

---

## 6. Cara Build Ulang (Ringkasan)

```bash
# 1. Kernel
export WATCOM=/path/to/open-watcom
export PATH=$PATH:$WATCOM/binl64
make all COMPILER=owlinux        # hasil: bin/kernel.sys, bin/kwc8632.sys, dll.
mv bin/kernel.sys bin/BURDAH.SYS # sesuai nama yang di-hardcode di sys/fdkrncfg.c

# 2. Portal (COMMAND.COM)
cd portal
bash build.sh watcom             # hasil: portal/command.com
cp command.com ../bin/COMMAND.COM
cd ..

# 3. (Opsional) Boot splash Rencana A
cd tools/splash
nasm -f bin splash.asm -o splash.com
cp splash.com ../../bin/SPLASH.COM
cd ../..

# 4. Floppy image 1.44MB
dd if=/dev/zero of=burdahdos.img bs=1024 count=1440
mformat -i burdahdos.img -f 1440 -v BURDAH ::
# tulis boot sector (boot/fat12com.bin, menjaga BPB dari mformat)
dd if=boot/fat12com.bin of=burdahdos.img bs=1 count=3 conv=notrunc
dd if=boot/fat12com.bin of=burdahdos.img bs=1 skip=62 seek=62 count=450 conv=notrunc
mcopy -i burdahdos.img -o bin/BURDAH.SYS bin/COMMAND.COM bin/country.sys \
  bin/config.sys bin/bbauto.bat bin/sys.com bin/SPLASH.COM ::
mattrib -i burdahdos.img +s +h ::BURDAH.SYS ::COMMAND.COM
```

Detail lengkap tiap langkah (termasuk kenapa submodule harus di-clone
manual, dan daftar bug build yang perlu diperbaiki di source sebelum
langkah di atas jalan mulus) ada di §2, §3, dan subseksi "Sesi Build
Pertama" di §4 dokumen ini.
