/* ===================================================================
 * detect_board.c
 * Deteksi nama motherboard via SMBIOS (System Management BIOS).
 *
 * SMBIOS baru mulai umum ada sekitar 1996+ -- mesin lebih tua (atau
 * BIOS murahan) mungkin tidak punya sama sekali, dan itu WAJAR
 * (status UNSUPPORTED), bukan bug.
 *
 * FASE 2: parsing tabel struct SMBIOS penuh, cari struct Type 2
 * (Baseboard Information) untuk Manufacturer + Product Name, dengan
 * fallback ke Type 1 (System Information) kalau Type 2 tidak ada
 * (umum di board generik/eval yang tidak mengisi Type 2).
 *
 * KETERBATASAN YANG DIKETAHUI (fase 3, belum diimplementasi):
 * Field "Structure Table Address" di entry point SMBIOS adalah alamat
 * fisik 32-bit yang BISA berada di atas batas 1MB real-mode. Kode ini
 * hanya bisa membaca tabel kalau alamatnya < 1MB (kasus umum di
 * kebanyakan BIOS yang sengaja menaruh tabel SMBIOS rendah demi
 * kompatibilitas DOS real-mode -- termasuk kemungkinan besar berlaku
 * di BIOS-BIOS yang diemulasikan 86Box). Kalau di atas 1MB, field
 * MACHINE akan menampilkan UNSUPPORTED dengan pesan yang jelas,
 * bukan crash atau data salah.
 * =================================================================== */

#include "detect_board.h"
#include <dos.h>
#include <string.h>
#include <stdio.h>

#pragma pack(push, 1)
typedef struct {
    unsigned char  anchor[4];      /* "_SM_" */
    unsigned char  checksum;
    unsigned char  length;
    unsigned char  major_ver;
    unsigned char  minor_ver;
    unsigned short max_struct_size;
    unsigned char  revision;
    unsigned char  formatted[5];
    unsigned char  dmi_anchor[5];  /* "_DMI_" */
    unsigned char  dmi_checksum;
    unsigned short table_length;
    unsigned long  table_address;
    unsigned short struct_count;
    unsigned char  bcd_revision;
} smbios_entry_t;

typedef struct {
    unsigned char  type;
    unsigned char  length;
    unsigned short handle;
} smbios_header_t;
#pragma pack(pop)

/* Ambil string ke-`index` (1-based, sesuai spec SMBIOS) dari blok
 * string yang mengikuti struct terformat, dipisah NUL, diakhiri NUL
 * ganda. index==0 berarti "field ini memang tak punya string". */
static int smbios_get_string(unsigned char far *strings, unsigned char index,
                              char *out, int out_size)
{
    unsigned char far *p = strings;
    unsigned char n = 1;
    int i;

    if (index == 0) return 0;

    while (*p != 0) {
        if (n == index) {
            for (i = 0; i < out_size - 1 && p[i] != 0; i++) out[i] = p[i];
            out[i] = '\0';
            return 1;
        }
        while (*p != 0) p++;
        p++; /* lewati NUL, mulai string berikutnya */
        n++;
    }
    return 0; /* index di luar jangkauan string-set */
}

void detect_board(board_info_t *out)
{
    unsigned char far *scan;
    smbios_entry_t far *entry = NULL;
    unsigned offset;

    memset(out, 0, sizeof(*out));

    /* --- Cari entry point "_SM_" di ROM F0000h-FFFFFh --- */
    for (offset = 0; offset < 0xFFF0U; offset += 16) {
        unsigned char far *p = (unsigned char far *)MK_FP(0xF000, offset);
        if (p[0] == '_' && p[1] == 'S' && p[2] == 'M' && p[3] == '_') {
            entry = (smbios_entry_t far *)p;
            break;
        }
    }

    if (entry == NULL) {
        strcpy(out->vendor_name.text, "N/A");
        out->vendor_name.status = STATUS_UNSUPPORTED; /* BIOS tanpa SMBIOS, wajar utk mesin lama */
        return;
    }

    {
        unsigned long table_addr = entry->table_address;
        unsigned table_len = entry->table_length;

        if (table_addr >= 0x100000UL) {
            /* Tabel struct di atas 1MB -- butuh unreal mode / INT15h
             * move block yang belum diimplementasi (fase 3). */
            strcpy(out->vendor_name.text, "N/A (SMBIOS table > 1MB, not supported yet)");
            out->vendor_name.status = STATUS_UNSUPPORTED;
            return;
        }

        {
            unsigned seg = (unsigned)(table_addr >> 4);
            unsigned off = (unsigned)(table_addr & 0xF);
            unsigned char far *table = (unsigned char far *)MK_FP(seg, off);
            unsigned pos = 0;
            char manuf[32], product[32];
            char manuf1[32], product1[32];
            int found_type2 = 0, found_type1 = 0;

            manuf[0] = product[0] = manuf1[0] = product1[0] = '\0';

            if ((unsigned long)off + table_len > 0xFFFFUL) {
                strcpy(out->vendor_name.text, "N/A (table out segment limit)");
                out->vendor_name.status = STATUS_FAIL;
                return;
            }

            while (pos + sizeof(smbios_header_t) <= table_len) {
                smbios_header_t far *hdr = (smbios_header_t far *)(table + pos);
                unsigned char far *fmt = table + pos;
                unsigned char far *strings;

                if (hdr->length < sizeof(smbios_header_t)) break; /* data korup */
                strings = table + pos + hdr->length;

                if (hdr->type == 2 && !found_type2) {
                    unsigned char manuf_idx = fmt[4];
                    unsigned char prod_idx  = fmt[5];
                    smbios_get_string(strings, manuf_idx, manuf, sizeof(manuf));
                    smbios_get_string(strings, prod_idx, product, sizeof(product));
                    found_type2 = 1;
                } else if (hdr->type == 1 && !found_type1) {
                    unsigned char manuf_idx = fmt[4];
                    unsigned char prod_idx  = fmt[5];
                    smbios_get_string(strings, manuf_idx, manuf1, sizeof(manuf1));
                    smbios_get_string(strings, prod_idx, product1, sizeof(product1));
                    found_type1 = 1;
                }

                if (hdr->type == 127) break; /* End-of-Table marker */

                /* Lompat lewati string-set (diakhiri NUL ganda) untuk
                 * cari posisi struct berikutnya. */
                {
                    unsigned char far *p2 = strings;
                    if (p2[0] == 0 && p2[1] == 0) {
                        pos += hdr->length + 2; /* tak ada string sama sekali */
                    } else {
                        while (!(p2[0] == 0 && p2[1] == 0)) p2++;
                        pos = (unsigned)(p2 - table) + 2;
                    }
                }

                if (found_type2) break; /* prioritas Type 2, cukup berhenti */
            }

            if (found_type2 && (manuf[0] || product[0])) {
                sprintf(out->vendor_name.text, "%s %s", manuf, product);
                out->vendor_name.status = STATUS_OK;
            } else if (found_type1 && (manuf1[0] || product1[0])) {
                sprintf(out->vendor_name.text, "%s %s", manuf1, product1);
                out->vendor_name.status = STATUS_OK;
            } else {
                strcpy(out->vendor_name.text, "N/A (struct Type 1/2 not found)");
                out->vendor_name.status = STATUS_NOT_DETECTED;
            }
        }
    }
}
