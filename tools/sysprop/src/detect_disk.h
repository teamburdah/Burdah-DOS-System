#ifndef DETECT_DISK_H
#define DETECT_DISK_H

#include "sysinfo.h"

/* Mengisi `out` dengan ukuran total HDD pertama (INT 13h AH=08h) dan
 * daftar partisi primer dari MBR. Nama model drive fisik (via ATA
 * IDENTIFY) masih TODO fase 2. */
void detect_disk(disk_info_t *out);

#endif /* DETECT_DISK_H */
