#ifndef DETECT_MEM_H
#define DETECT_MEM_H

#include "sysinfo.h"

/* Mengisi `out` dengan RAM conventional (INT 12h) dan extended
 * (INT 15h AX=E801h, fallback AH=88h). Bekerja tanpa driver memori
 * tambahan apapun (tidak butuh HIMEM.SYS). */
void detect_mem(mem_info_t *out);

#endif /* DETECT_MEM_H */
