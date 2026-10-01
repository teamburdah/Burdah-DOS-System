#ifndef DETECT_CPU_X86_H
#define DETECT_CPU_X86_H

#include "sysinfo.h"

/* ===================================================================
 * detect_cpu_x86.h
 *
 * Fungsi-fungsi yang MEMBUTUHKAN instruksi 386+/CPUID/RDTSC secara
 * eksplisit di kode mesinnya. Dipisah ke file/objek TERSENDIRI karena
 * ditemukan lewat pengujian compile nyata: Open Watcom assembler
 * MENOLAK instruksi di luar target CPU yang dipilih saat compile
 * (-0/-3/-5 dst) -- ini BUKAN cuma soal optimisasi/warning, tapi
 * validasi keras di compile time (error, bukan sekadar peringatan).
 *
 * Jadi file detect_cpu_x86.c WAJIB dikompilasi dengan flag target CPU
 * lebih tinggi (dipakai: -5, target Pentium, supaya CPUID & RDTSC
 * keduanya diterima assembler), SEMENTARA detect_cpu.c (pemanggilnya)
 * dan semua file lain di proyek ini tetap dikompilasi -0 supaya aman
 * dijalankan di 8086 murni.
 *
 * Instruksi 386+/CPUID/RDTSC ADA secara fisik di file .obj hasil
 * compile modul ini, tapi TIDAK PERNAH dieksekusi di CPU yang tak
 * mendukungnya -- karena caller (detect_cpu.c) hanya memanggil
 * fungsi-fungsi ini SETELAH runtime check FLAGS-based memastikan CPU
 * memang >= 80386 (dan >= Pentium khusus utk RDTSC). Ini prinsip yang
 * sama dengan compatibility gate dari awal: percabangan dilakukan di
 * RUNTIME, bukan compile-time -- yang berubah cuma lokasi file
 * sumbernya, supaya assembler mengizinkan instruksinya ditulis.
 * =================================================================== */

/* Tahap 3 & 4 dari flags_probe() -- uji AC flag (bit18) & ID flag
 * (bit21) di EFLAGS 32-bit. HANYA aman dipanggil setelah caller
 * memastikan CPU >= 80386 (via cpu_supports_386_instructions() di
 * detect_cpu.c, BUKAN lagi via tes bit IOPL/NT FLAGS -- lihat catatan
 * di detect_cpu.c kenapa tes lama itu tak lagi dipakai).
 * Return: 386, 486, atau 586 (586 berarti CPUID didukung). */
int flags_probe_extended(void);

/* Probe MINIMAL: coba jalankan pushfd/popfd (instruksi 386+ murni,
 * TIDAK ADA di 8086/80186/80286). Dipanggil HANYA lewat pembungkus
 * setjmp/longjmp + handler INT 6 di detect_cpu.c -- kalau CPU asli
 * tak mendukungnya, CPU akan memicu invalid-opcode fault (INT 6) yang
 * ditangkap dengan aman oleh caller, BUKAN hang/crash. Fungsi ini
 * sendiri TIDAK melakukan pengecekan apapun -- dia HANYA berguna
 * kalau caller sudah memasang jaring pengaman fault sebelum
 * memanggilnya. */
void probe_pushfd_raw(void);

/* Isi vendor & brand string CPU via CPUID. HANYA aman dipanggil kalau
 * flags_probe_extended() sudah mengembalikan 586 (CPUID didukung). */
void read_cpuid_strings(cpu_info_t *out);

/* Cek bit TSC (bit4) di CPUID leaf 1 EDX. HANYA aman dipanggil kalau
 * CPUID sudah dipastikan didukung. */
int cpu_has_tsc(void);

/* Ukur clock speed via RDTSC selama `target_ticks` PIT tick. HANYA
 * aman dipanggil kalau cpu_has_tsc() sudah mengembalikan 1. */
unsigned long measure_mhz_rdtsc(unsigned long target_ticks);

#endif /* DETECT_CPU_X86_H */
