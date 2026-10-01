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

## 9. Temuan dari kompilasi & pengujian nyata (Open Watcom + DOSBox)

Setelah source ini benar-benar dikompilasi dengan Open Watcom dan diuji
jalan di DOSBox (bukan cuma ditulis berdasar dokumentasi), ditemukan
beberapa masalah nyata yang tidak mungkin ketahuan tanpa proses ini:

**a) Assembler Open Watcom MENOLAK instruksi di luar target CPU yang
dipilih saat compile -- ini validasi keras, bukan sekadar optimisasi.**
Awalnya `detect_cpu.c` (compile target `-0`, 8086) berisi kode
`pushfd`/`popfd`/register 32-bit/`cpuid`/`rdtsc` untuk tahap deteksi
386+ -- asumsinya percabangan runtime sudah cukup melindungi CPU lama
dari instruksi yang tak didukungnya. Ternyata SALAH: compiler menolak
instruksi itu di level assembler, bukan di runtime. Solusi: kode yang
butuh 386+/CPUID/RDTSC dipindah ke file terpisah (`detect_cpu_x86.c`),
dikompilasi dengan target CPU lebih tinggi (`-5`, Pentium), sementara
`detect_cpu.c` sendiri tetap `-0`. Prinsip "percabangan di runtime"
tetap berlaku, tapi sekarang di level FILE juga, bukan cuma logika.

**b) Nama variabel yang bentrok dengan operator/register assembler
GAGAL COMPILE (aman) atau DIAM-DIAM SALAH (berbahaya), tergantung
jenisnya.** Ditemukan dua kasus:
- `seg`/`off` sebagai nama variabel di `detect_os.c` -- bentrok dengan
  operator `SEG`/`OFFSET` bawaan inline assembler Watcom, menyebabkan
  ERROR compile (`Assembler error: 'Operand is expected'`). Aman
  karena ketahuan langsung saat compile.
- `ch` sebagai nama PARAMETER FUNGSI di `render.c` -- bentrok dengan
  register `CH`. Ini JAUH LEBIH BERBAHAYA: compile SUKSES tanpa
  peringatan apapun, tapi assembler diam-diam membaca register CH
  (berisi sampah) alih-alih parameter kita, membuat karakter yang
  ditulis ke layar jadi acak. Baru ketahuan setelah dijalankan nyata
  di DOSBox (layar berantakan). Pelajaran: SEMUA nama variabel/
  parameter yang dipakai di dalam blok `_asm` harus dihindari dari
  nama register (al/ah/ax/bl/bh/bx/cl/ch/cx/dl/dh/dx/si/di/sp/bp/
  cs/ds/es/ss) dan operator (seg/offset/ptr/near/far/dst).

**c) `[nama_pointer]` di dalam `_asm` TIDAK men-dereference pointer --
ini membaca byte dari ALAMAT VARIABEL POINTER itu sendiri, bukan data
yang ditunjuknya.** Ditemukan di `put_string_plain()` (`render.c`):
`mov al, byte ptr [s]` (dengan `s` adalah `const char *`) diam-diam
salah, membaca low-byte dari NILAI POINTER s (yang berubah seiring
`s++`), bukan karakter string yang ditunjuk s. Ini SANGAT berbahaya
karena compile sukses, dan hasilnya (byte-byte alamat yang kebetulan
berurutan) terlihat SEPERTI data valid, bukan crash jelas -- baru
ketahuan dari POLA ANEH di output (separator " : " tercetak jadi
"./0", pola byte berurutan 0x2E/0x2F/0x30). Perbaikan: untuk pointer
near, ambil nilai `*s` di C biasa dulu ke variabel lokal, baru pakai
variabel itu di `_asm` -- hindari dereference pointer langsung di
dalam blok assembly kalau bisa. (Catatan: pola `les bx, far_ptr_var`
yang dipakai di modul lain BERBEDA dan TETAP BENAR, karena semantik
instruksi LES/LDS memang "muat far pointer DARI memori tempat pointer
itu disimpan" -- bukan dereference isi yang ditunjuknya.)

**d) `MK_FP`/`FP_SEG`/`FP_OFF` ada di `<i86.h>`, BUKAN `<dos.h>`**
di versi Open Watcom yang dipakai -- assumsi awal (umum di banyak
compiler DOS lama seperti Turbo C) ternyata tidak berlaku di sini.
Diperbaiki dengan menambah `#include <i86.h>` di `detect_os.c` dan
`detect_board.c`.

**e) DOSBox versi bawaan (`mount c:` tanpa boot FreeDOS asli) memakai
kernel DOS internalnya sendiri yang SANGAT minimal -- BUKAN FreeDOS
sungguhan.** Saat program memanggil `INT 21h AX=33FFh` (ekstensi versi
kernel FreeDOS), kernel internal DOSBox melaporkan
`"Exit to error: DOS:Illegal 0x33 Call FF"` dan MENGHENTIKAN eksekusi
-- padahal sudah diverifikasi dari source kernel FreeDOS asli
(github.com/FDOS/kernel) bahwa fungsi ini valid, didukung FreeDOS/
RxDOS/EDR-DOS/lDOS, dan cara pemanggilannya (DX=0 dulu, cek segment
hasil) sudah benar sesuai rekomendasi resmi pengembang FreeDOS. Ini
KETERBATASAN LINGKUNGAN UJI (DOSBox internal kernel), bukan bug kode
kita -- tapi berarti pengujian cepat via `dosbox` + `mount c:` biasa
TIDAK REPRESENTATIF untuk fitur ini. Target uji yang benar (86Box +
FreeDOS 1.4 boot sungguhan, sesuai rencana) seharusnya menangani
fungsi ini dengan baik.

**Kesimpulan metodologis:** poin (a)-(d) TIDAK MUNGKIN ditemukan hanya
dari membaca dokumentasi atau menulis kode "yang kelihatan benar" --
harus benar-benar dikompilasi dan dijalankan. Ini mengonfirmasi
prinsip yang dipegang sejak awal proyek (lihat PROGRESS.md): kode yang
melibatkan operasi assembly/hardware-level butuh verifikasi nyata,
bukan cuma review manual.

## 10. Perbaikan gerbang deteksi 286-vs-386+ (temuan lapangan: 86Box + QEMU)

**Laporan user**: diuji lintas beberapa profil mesin di 86Box dan di
QEMU, CPU HAMPIR SELALU terdeteksi sbg generik "Intel 80286 atau
kompatibel" -- bahkan pada mesin yang seharusnya jauh lebih baru/cepat
(estimasi clock speed di QEMU mencapai ~1GHz, tapi nama CPU tetap
"80286"). Ini BUKAN sekadar kekurangan database nama (spt dugaan awal
user) -- ini bug nyata di GERBANG deteksi generasi CPU itu sendiri:
selama tahap 2 (286 vs 386+) salah simpul ke "286", kode TIDAK PERNAH
mencapai cabang CPUID/RDTSC sama sekali, apapun CPU sebenarnya.

**Analisis akar masalah**: tahap 2 lama menguji kuirk FLAGS bit 12-15
(coba SET bit IOPL/NT via `POPF` 16-bit, cek apakah "nempel" saat
dibaca ulang via `PUSHF`). Teknik ini klasik dan VALID di hardware x86
asli, tapi diduga kuat TIDAK DIEMULASIKAN SECARA AKURAT oleh banyak
hypervisor/emulator -- IOPL adalah flag terkait *privilege* I/O, target
wajar untuk dimediasi/dibatasi hypervisor demi isolasi keamanan, BEDA
dari instruksi CPU sungguhan (seperti `pushfd`) yang WAJIB diemulasikan
benar oleh emulator manapun supaya software lain (yg jauh lebih umum
drpd utility deteksi CPU jadul macam kita) tetap berjalan normal.

**Perbaikan**: tahap 2 diganti total. Alih-alih menguji KUIRK NILAI
FLAGS, sekarang kita LANGSUNG MENCOBA MENGEKSEKUSI instruksi 386+
sungguhan (`pushfd`/`popfd`, via fungsi minimal `probe_pushfd_raw()` di
`detect_cpu_x86.c`), dilindungi `setjmp`/`longjmp` (dari `<setjmp.h>`
standar) + handler `INT 6` (invalid opcode) custom yang dipasang
sementara via `_dos_setvect`/`_dos_getvect`. Kalau CPU sungguhan tak
mengenal instruksi itu (8086/80186/80286 asli), CPU memicu exception
INT 6 SEBELUM instruksi sempat mengubah apapun (fault di tahap decode);
handler kita menangkapnya lewat `longjmp()` dengan bersih -- pola
"longjmp manual" klasik utk mencoba instruksi yang MUNGKIN tak
didukung TANPA risiko hang/crash. INT 6 adalah CPU exception murni
(bukan hardware IRQ), jadi tak perlu EOI ke PIC 8259.

**Kenapa `setjmp`/`longjmp`, bukan manipulasi stack manual?** Opsi
awal yang dipertimbangkan (restore SS:SP manual + `jmp` langsung ke
titik lanjutan) jauh lebih rawan salah dan susah diverifikasi
kebenarannya tanpa akses hardware 286 asli. `setjmp`/`longjmp` adalah
fasilitas C standar yang sudah teruji, jauh lebih aman untuk pola
"coba jalankan sesuatu yang mungkin gagal, tangkap dgn bersih" ini.

**Verifikasi**: diuji nyata di DOSBox (yg SELALU emulasikan CPU
setara 386+ ke atas, tak pernah benar-benar 286) -- SEBELUM perbaikan,
app SELALU melaporkan "Intel 80286 atau kompatibel" bahkan di DOSBox.
SETELAH perbaikan, app berhasil mencapai cabang CPUID dan melaporkan
"GenuineIntel Family 4 Model 0" (identitas CPUID sintetis yang dipakai
DOSBox utk cputype=auto) -- mengonfirmasi gerbang deteksi sekarang
benar mengenali CPU 386+ di lingkungan virtualisasi. Program tetap
selesai tanpa hang/crash.

**KETERBATASAN VERIFIKASI YANG MASIH ADA**: DOSBox tidak bisa
mengemulasikan CPU 286 asli (cputype minimumnya sudah 386-class), jadi
JALUR FAULT (longjmp dipanggil krn CPU sungguhan menolak pushfd) BELUM
PERNAH benar-benar teruji jalan -- yang teruji baru "jalur sukses"
(CPU mendukung, tidak fault). Jalur fault ini PENTING divalidasi di
86Box dgn profil mesin 286 asli (86Box mengemulasikan CPU dgn akurasi
tinggi, TERMASUK kemungkinan perilaku invalid-opcode yg benar utk 286
sungguhan) sebelum dianggap 100% aman di semua kasus. Kalau app hang
atau crash spesifik di mesin 286 86Box, ini kemungkinan pertama yang
perlu dicurigai.

## 11. Tabel Family/Model CPUID untuk nama CPU lebih spesifik

Ditambahkan `cpu_model_table[]` di `detect_cpu_x86.c` -- dipakai HANYA
kalau brand string (leaf 0x80000002-4) tak tersedia (umum utk CPU
generasi Pentium klasik s.d. awal Pentium II/Celeron, sebelum brand
string jadi standar luas). Family/Model diambil dari CPUID leaf 1
(EAX), dicocokkan ke tabel referensi kecil (Intel Family 5/6 klasik +
AMD K5/K6/K7 awal). Kalau kombinasi tak dikenali, TETAP ditampilkan
apa adanya ("Vendor Family X Model Y") dengan status NA -- TIDAK
PERNAH mengarang nama yang tak yakin benar, konsisten dgn prinsip
proyek. Tabel ini SENGAJA tidak lengkap (mustahil mencakup semua
clone/varian/stepping historis) dan bisa ditambah bertahap kalau
ditemukan kombinasi baru yang layak didokumentasikan.

**Catatan soal permintaan awal user** (deteksi 8086 vs 8088, dan nama
lebih spesifik utk 286/386/486 generik): TIDAK diimplementasikan di
tahap ini. 8086 vs 8088 secara teknis butuh mengukur timing relatif
bus 8-bit vs 16-bit (teknik yang ada tidak 100% andal lintas clone
chip) -- berisiko memberi kepastian palsu. Nama spesifik model 286/
386SX vs DX/486SX vs DX (tanpa CPUID) butuh teknik tambahan (mis. tes
keberadaan FPU internal via handler INT 7 utk beda SX/DX, mirip pola
INT 6 di atas) yang BELUM diimplementasikan -- kandidat kuat utk fase
berikutnya, TAPI perlu kehati-hatian & verifikasi yang sama spt INT 6
di atas sebelum dianggap aman.
