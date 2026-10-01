#ifndef DETECT_CPU_H
#define DETECT_CPU_H

#include "sysinfo.h"

/* Mengisi `out` dengan hasil deteksi CPU: generasi (via FLAGS-probing,
 * jalan dari 8086 s.d. Pentium+), vendor/nama (via CPUID kalau ada),
 * dan estimasi clock speed. Tidak pernah gagal/crash -- field yang
 * tak terdeteksi diisi status_t yang sesuai, bukan dibiarkan kosong. */
void detect_cpu(cpu_info_t *out);

#endif /* DETECT_CPU_H */
