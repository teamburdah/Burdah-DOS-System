/* ===================================================================
 * detect_os.c
 *
 * Deteksi nama & versi sistem operasi DOS yang sedang berjalan.
 *
 * FreeDOS punya ekstensi tak-resmi tapi sudah lama didokumentasikan
 * developer-nya: INT 21h AX=33FFh, dipanggil dengan DX=0. Kalau
 * kernel yang berjalan adalah FreeDOS, DX:AX akan diisi far pointer
 * ke string ASCIZ berisi versi kernel ASLI (mis. "2045"), BUKAN versi
 * DOS yang di-emulasikan untuk kompatibilitas software (yang bisa
 * dipengaruhi SETVER dan biasanya dilaporkan sbg 7.10 dsb).
 * 
 * CATATAN: OEM ID Burdah adalah 0xBB, jadi Burdah-DOS-System juga
 * didukung (proyek ini untuk Burdah-DOS-System)
 *
 * Cara deteksi dukungan: panggil dengan DX=0 dulu. Kalau kernel TIDAK
 * mengenal fungsi ini (bukan FreeDOS), register DX akan TETAP 0
 * karena fungsi tak dikenal diabaikan begitu saja oleh int21 dispatch
 * table DOS pada umumnya (tidak mengubah register). Kalau FreeDOS,
 * DX akan diisi segment pointer yang valid (bukan 0).
 *
 * Kalau ekstensi ini tak ada (non-FreeDOS), fallback ke fungsi standar
 * INT 21h AH=30h yang ada sejak DOS 2.0 -- memberi versi + OEM ID
 * (00h=PC-DOS, FFh=MS-DOS, lainnya=OEM lain/tak diketahui).
 * =================================================================== */

#include "detect_os.h"
#include <dos.h>
#include <string.h>
#include <stdio.h>

void detect_os(field_t *out)
{
    unsigned seg = 0, off = 0;

    memset(out, 0, sizeof(*out));

    /* --- Coba ekstensi FreeDOS: INT 21h AX=33FFh, DX=0 --- */
    _asm {
        mov ax, 33FFh
        mov dx, 0
        int 21h
        mov off, ax
        mov seg, dx
    }

    if (seg != 0) {
        char far *ver_str = (char far *)MK_FP(seg, off);
        char buf[40];
        unsigned i;

        for (i = 0; i < sizeof(buf) - 1 && ver_str[i] != '\0'; i++) {
            buf[i] = ver_str[i];
        }
        buf[i] = '\0';

        if (buf[0] != '\0') {
            sprintf(out->text, "FreeDOS (kernel %s)", buf);
            out->status = STATUS_OK;
            return;
        }
        /* String kosong walau pointer valid -- jarang terjadi, tapi
         * tetap fallback ke deteksi standar di bawah kalau begitu. */
    }

    /* --- Fallback: fungsi standar AH=30h, ada sejak DOS 2.0 ---
     * Ini melaporkan versi DOS yang DI-EMULASIKAN (bisa dipengaruhi
     * SETVER), bukan versi kernel asli -- tapi cukup memadai untuk
     * kasus non-FreeDOS (MS-DOS, PC-DOS, DR-DOS, dsb). */
    {
        unsigned char major = 0, minor = 0, oem = 0;

        _asm {
            mov ax, 3000h
            int 21h
            mov major, al
            mov minor, ah
            mov oem, bh
        }

        if (major == 0 && minor == 0) {
            /* DOS versi sangat lama (<2.0) tidak mengimplementasikan
             * fungsi ini sama sekali dan akan selalu return 0. */
            strcpy(out->text, "N/A (DOS < 2.0 or unknown)");
            out->status = STATUS_UNSUPPORTED;
            return;
        }

        {
            const char *oem_name;
            switch (oem) {
                case 0x00: oem_name = "PC-DOS"; break;
                case 0xFF: oem_name = "MS-DOS"; break;
				case 0xBB: oem_name = "Burdah-DOS-System"; break;
                default:   oem_name = "DOS (OEM tak dikenali)"; break;
            }
            sprintf(out->text, "%s %u.%02u", oem_name, major, minor);
            out->status = STATUS_OK;
        }
    }
}
