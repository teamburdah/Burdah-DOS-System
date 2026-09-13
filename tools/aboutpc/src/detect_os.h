#ifndef DETECT_OS_H
#define DETECT_OS_H

#include "sysinfo.h"

/* Mengisi `out` dengan nama & versi sistem operasi DOS yang sedang
 * berjalan. Mengutamakan ekstensi khusus kernel FreeDOS (INT 21h
 * AX=33FFh) yang mengembalikan string versi KERNEL ASLI (tidak
 * terpengaruh SETVER), lalu fallback ke fungsi standar INT 21h
 * AH=30h (versi DOS yang di-emulasikan) kalau ekstensi itu tak ada
 * -- berarti bukan FreeDOS (MS-DOS asli, DR-DOS, dsb). */
void detect_os(field_t *out);

#endif /* DETECT_OS_H */
