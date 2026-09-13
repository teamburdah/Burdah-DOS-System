/* ===================================================================
 * detect_video.c
 * Deteksi jenis adapter video via BIOS INT 10h, dan (FASE 2) resolusi
 * monitor asli via VESA VBE (Video BIOS Extension) DDC + parsing EDID.
 *
 * EDID adalah data 128 byte yang dikirim monitor ke graphics card
 * lewat jalur DDC (Display Data Channel, protokol I2C di kabel VGA/
 * DVI/HDMI pin tertentu). Card & BIOS-nya HARUS mendukung VBE fungsi
 * 4Fh/15h untuk membaca ini -- kalau tidak, resolusi ditandai
 * UNSUPPORTED (bukan error, memang fiturnya tak ada).
 * =================================================================== */

#include "detect_video.h"
#include <dos.h>
#include <string.h>
#include <stdio.h>

/* --- VESA VBE: cek dukungan DDC (Display Data Channel) --- */
static int vbe_ddc_supported(void)
{
    unsigned ax_result = 0, bx_result = 0;

    _asm {
        mov ax, 4F15h
        mov bl, 00h        ; subfungsi: report DDC capabilities
        mov cx, 0
        mov dx, 0
        int 10h
        mov ax_result, ax
        mov bx_result, bx
    }

    if ((ax_result & 0x00FF) != 0x004F) return 0; /* fungsi VBE tak ada sama sekali */
    if ((ax_result & 0xFF00) != 0x0000) return 0; /* fungsi ada tapi gagal dipanggil */

    return (bx_result & 0x03) != 0; /* bit0/1 BL hasil = dukung DDC1/DDC2 */
}

/* --- VESA VBE: baca EDID block 0 (128 byte) dari monitor --- */
static int vbe_read_edid(unsigned char edid[128])
{
    unsigned ax_result = 0;
    void far *buf_ptr = (void far *)edid;

    _asm {
        mov ax, 4F15h
        mov bl, 01h        ; subfungsi: read EDID block
        mov cx, 0          ; controller unit 0
        mov dx, 0          ; EDID block 0
        les di, buf_ptr
        int 10h
        mov ax_result, ax
    }

    return ((ax_result & 0x00FF) == 0x004F && (ax_result & 0xFF00) == 0x0000);
}

static int edid_checksum_ok(const unsigned char edid[128])
{
    unsigned sum = 0;
    int i;
    for (i = 0; i < 128; i++) sum += edid[i];
    return (sum & 0xFF) == 0;
}

/* Header EDID selalu diawali pola tetap 00 FF FF FF FF FF FF 00 --
 * dipakai untuk validasi cepat sebelum parsing lebih jauh. */
static int edid_header_valid(const unsigned char edid[128])
{
    static const unsigned char expected[8] =
        { 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };
    return memcmp(edid, expected, 8) == 0;
}

/* Ambil resolusi dari Detailed Timing Descriptor pertama (offset 54,
 * 18 byte) -- ini konvensinya berisi mode "preferred" monitor.
 * Layout bit H-active/V-active: byte rendah di offset+2/+5, nibble
 * tinggi (bit 8-11) di offset+4/+7 (upper nibble masing-masing). */
static void edid_get_resolution(const unsigned char edid[128],
                                 unsigned *h_active, unsigned *v_active)
{
    const unsigned char *dtd = edid + 54;
    *h_active = ((unsigned)(dtd[4] & 0xF0) << 4) | dtd[2];
    *v_active = ((unsigned)(dtd[7] & 0xF0) << 4) | dtd[5];
}

void detect_video(video_info_t *out)
{
    unsigned char mode = 0;
    unsigned char al_result = 0;

    memset(out, 0, sizeof(*out));

    /* Mode video aktif -- fungsi paling dasar INT 10h, ada sejak
     * adapter CGA/MDA generasi paling awal. */
    _asm {
        mov ah, 0Fh
        int 10h
        mov mode, al
    }

    /* Deteksi kelas VGA via INT 10h AX=1A00h. Fungsi ini baru ada di
     * BIOS adapter VGA ke atas -- kalau AL tidak berubah jadi 1Ah
     * setelah panggilan, berarti adapter di bawah VGA. */
    _asm {
        mov ax, 1A00h
        int 10h
        mov al_result, al
    }

    if (al_result == 0x1A) {
        strcpy(out->adapter.text, "VGA or newest");
        out->adapter.status = STATUS_OK;

        if (vbe_ddc_supported()) {
            unsigned char edid[128];
            if (vbe_read_edid(edid) && edid_header_valid(edid) &&
                edid_checksum_ok(edid)) {
                unsigned h_active = 0, v_active = 0;
                edid_get_resolution(edid, &h_active, &v_active);

                if (h_active > 0 && v_active > 0) {
                    sprintf(out->resolution.text, "%u x %u", h_active, v_active);
                    out->resolution.status = STATUS_OK;
                } else {
                    strcpy(out->resolution.text, "N/A (timing descriptor null)");
                    out->resolution.status = STATUS_FAIL;
                }
            } else {
                strcpy(out->resolution.text, "N/A (EDID error/crash)");
                out->resolution.status = STATUS_FAIL;
            }
        } else {
            out->resolution.text[0] = '\0';
            out->resolution.status = STATUS_UNSUPPORTED; /* monitor/card tak dukung DDC */
        }
    } else {
        /* Fallback klasifikasi kasar berdasar nomor mode video aktif. */
        const char *name;
        switch (mode) {
            case 0x07:
                name = "MDA (monochrome)";
                break;
            case 0x04:
            case 0x05:
            case 0x06:
                name = "CGA";
                break;
            case 0x0D:
            case 0x0E:
            case 0x0F:
            case 0x10:
                name = "EGA";
                break;
            default:
                name = "Unknown video adapter";
                break;
        }
        strcpy(out->adapter.text, name);
        out->adapter.status = STATUS_OK;

        /* Adapter di bawah VGA memang tak punya jalur DDC/EDID --
         * ini N/A, bukan kegagalan deteksi. */
        out->resolution.text[0] = '\0';
        out->resolution.status = STATUS_NA;
    }
}
