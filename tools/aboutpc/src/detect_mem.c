/* ===================================================================
 * detect_mem.c
 * Deteksi RAM conventional & extended lewat BIOS interrupt murni --
 * tidak butuh driver tambahan, jalan di semua generasi CPU 8086+.
 * =================================================================== */

#include "detect_mem.h"
#include <dos.h>
#include <stdio.h>
#include <string.h>

void detect_mem(mem_info_t *out)
{
    unsigned conv_kb = 0;
    unsigned long ext_kb = 0;

    memset(out, 0, sizeof(*out));

    /* --- Conventional memory ---
     * INT 12h: fungsi paling dasar, ada sejak IBM PC pertama.
     * Return: AX = jumlah KB kontinu dari alamat 0. */
    _asm {
        int 12h
        mov conv_kb, ax
    }
    sprintf(out->conventional.text, "%u KB", conv_kb);
    out->conventional.status = STATUS_OK;

    /* --- Extended memory ---
     * Coba INT 15h AX=E801h dulu (lebih akurat & mendukung >64MB),
     * baru fallback ke AH=88h (fungsi lawas, batas laporan lebih
     * kecil) kalau E801h tidak didukung BIOS. */
    {
        unsigned ax_e801 = 0, bx_e801 = 0;
        unsigned char cf = 1;

        _asm {
            mov ax, 0E801h
            int 15h
            jc  fail_e801
            mov ax_e801, ax
            mov bx_e801, bx
            mov cf, 0
        fail_e801:
        }

        if (!cf && (ax_e801 != 0 || bx_e801 != 0)) {
            /* AX = KB antara 1MB-16MB, BX = jumlah blok 64KB di atas 16MB */
            ext_kb = (unsigned long)ax_e801 + ((unsigned long)bx_e801 * 64UL);
        } else {
            unsigned ax_88 = 0;
            unsigned char cf88 = 1;
            _asm {
                mov ah, 88h
                int 15h
                jc  fail_88
                mov ax_88, ax
                mov cf88, 0
            fail_88:
            }
            if (!cf88) ext_kb = ax_88;
        }
    }

    if (ext_kb > 0) {
        sprintf(out->extended.text, "%lu MB", ext_kb / 1024UL);
        out->extended.status = STATUS_OK;
    } else {
        /* Wajar terjadi di mesin XT/AT murni tanpa memori di atas 1MB
         * -- ini bukan error, memang tidak ada extended memory. */
        strcpy(out->extended.text, "N/A");
        out->extended.status = STATUS_NA;
    }
}
