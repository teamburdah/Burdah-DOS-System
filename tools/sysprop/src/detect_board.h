#ifndef DETECT_BOARD_H
#define DETECT_BOARD_H

#include "sysinfo.h"

/* Mengisi `out` dengan nama motherboard via scan SMBIOS di ROM BIOS.
 * Parsing struct SMBIOS lengkap (Type 2 - Baseboard Info) masih
 * TODO fase 2 -- saat ini baru mendeteksi KEBERADAAN SMBIOS. */
void detect_board(board_info_t *out);

#endif /* DETECT_BOARD_H */
