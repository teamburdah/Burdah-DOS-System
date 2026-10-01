/* ===================================================================
 * detect_board.c
 * Deteksi nama motherboard via SMBIOS (System Management BIOS).
 *
 * SMBIOS baru mulai umum ada sekitar 1996+ -- mesin lebih tua (atau
 * BIOS murahan) mungkin tidak punya sama sekali, dan itu WAJAR
 * (status UNSUPPORTED), bukan bug.
 *
 * Parsing tabel struct SMBIOS: cari struct Type 2 (Baseboard
 * Information) untuk Manufacturer + Product Name, dengan fallback ke
 * Type 1 (System Information) kalau Type 2 tidak ada (umum di board
 * generik/eval yang tidak mengisi Type 2).
 *
 * FASE 3: field "Structure Table Address" di entry point SMBIOS
 * adalah alamat fisik 32-bit yang BISA berada di atas batas 1MB
 * real-mode. Kalau begitu, tabel dibaca via BIOS INT 15h AH=87h
 * ("move block") -- fungsi standar pra-XMS yang sudah ada sejak
 * 80286+ untuk menyalin data dari memori tinggi ke conventional
 * memory tanpa perlu unreal-mode trick manual. Kalau CPU/BIOS tak
 * mendukung fungsi ini (mis. mesin 8086/80186), fallback ke status
 * UNSUPPORTED yang jelas -- bukan crash.
 * =================================================================== */

#include "detect_board.h"
#include <dos.h>
#include <i86.h>
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

/* Format deskriptor GDT klasik yang dipakai INT 15h AH=87h -- BUKAN
 * format 386-extended penuh (byte 6-7 harus nol, tak ada bit limit
 * high-nibble/granularity krn fungsi ini mendahului era 386). */
typedef struct {
    unsigned short limit;
    unsigned short base_lo;
    unsigned char  base_mid;
    unsigned char  access;
    unsigned char  reserved1;
    unsigned char  reserved2;
} move_descriptor_t; /* 8 byte */

typedef struct {
    move_descriptor_t dummy;    /* 0: harus nol semua, wajib ada */
    move_descriptor_t gdt_self; /* 1: deskriptor GDT ini sendiri (opsional) */
    move_descriptor_t src;      /* 2: sumber data (memori tinggi, bisa >1MB) */
    move_descriptor_t dst;      /* 3: tujuan (buffer conventional memory) */
    move_descriptor_t bios_cs;  /* 4: dipakai internal oleh BIOS saat pindah mode */
    move_descriptor_t bios_ss;  /* 5: dipakai internal oleh BIOS saat pindah mode */
} move_gdt_t; /* total 48 byte */
#pragma pack(pop)

#define SMBIOS_MOVE_BUF_SIZE 2048 /* cukup besar utk kebanyakan tabel SMBIOS umum */

static void set_move_descriptor(move_descriptor_t *d, unsigned long base,
                                 unsigned limit, unsigned char access)
{
    d->limit     = limit;
    d->base_lo   = (unsigned short)(base & 0xFFFFUL);
    d->base_mid  = (unsigned char)((base >> 16) & 0xFFUL);
    d->access    = access;
    d->reserved1 = 0;
    d->reserved2 = 0;
}

/* Salin `length_bytes` byte dari alamat fisik 32-bit `src_addr` (bisa
 * di atas 1MB) ke buffer conventional memory `dest`, via BIOS INT 15h
 * AH=87h. Return 1 jika sukses, 0 jika gagal (CPU <286, BIOS tak
 * dukung fungsi ini, atau error lain dilaporkan lewat carry flag). */
static int bios_move_block(unsigned long src_addr, void far *dest,
                            unsigned length_bytes)
{
    static move_gdt_t gdt; /* static: hindari isu alignment/ukuran di stack */
    unsigned words = (length_bytes + 1) / 2; /* fungsi ini bekerja per word 16-bit */
    unsigned char cf = 1;
    void far *gdt_ptr = (void far *)&gdt;
    unsigned long dest_linear =
        ((unsigned long)FP_SEG(dest) << 4) + (unsigned long)FP_OFF(dest);
    unsigned limit_val = (length_bytes > 0) ? (unsigned)(length_bytes - 1) : 0;

    memset(&gdt, 0, sizeof(gdt));
    set_move_descriptor(&gdt.src, src_addr, limit_val, 0x93);      /* data, read-write, present */
    set_move_descriptor(&gdt.dst, dest_linear, limit_val, 0x93);
    /* gdt.bios_cs & gdt.bios_ss dibiarkan nol -- diisi BIOS sendiri
     * saat pemanggilan, sesuai spesifikasi fungsi ini. */

    _asm {
        push es
        push si
        les si, gdt_ptr
        mov cx, words
        mov ah, 87h
        int 15h
        jc  fail_move
        mov cf, 0
    fail_move:
        pop si
        pop es
    }

    return !cf;
}

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

/* Iterasi tabel struct SMBIOS (di `table`, sepanjang `table_len` byte)
 * cari Type 2 (Baseboard), fallback Type 1 (System). Dipakai baik utk
 * akses langsung (<1MB) maupun buffer hasil bios_move_block() (>1MB)
 * -- keduanya sama-sama diakses lewat far pointer biasa. */
static void parse_smbios_table(unsigned char far *table, unsigned table_len,
                                board_info_t *out)
{
    unsigned pos = 0;
    char manuf[32], product[32];
    char manuf1[32], product1[32];
    int found_type2 = 0, found_type1 = 0;

    manuf[0] = product[0] = manuf1[0] = product1[0] = '\0';

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

        /* Lompat lewati string-set (diakhiri NUL ganda) untuk cari
         * posisi struct berikutnya. */
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
        strcpy(out->vendor_name.text, "N/A (struct Type 1/2 tak ditemukan)");
        out->vendor_name.status = STATUS_NOT_DETECTED;
    }
}

void detect_board(board_info_t *out)
{
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

        if (table_addr < 0x100000UL) {
            /* --- Jalur cepat: tabel di bawah 1MB, akses langsung --- */
            unsigned seg = (unsigned)(table_addr >> 4);
            unsigned off = (unsigned)(table_addr & 0xF);
            unsigned char far *table = (unsigned char far *)MK_FP(seg, off);

            if ((unsigned long)off + table_len > 0xFFFFUL) {
                strcpy(out->vendor_name.text, "N/A (tabel melewati batas segmen)");
                out->vendor_name.status = STATUS_FAIL;
                return;
            }
            parse_smbios_table(table, table_len, out);
            return;
        }

        /* --- Jalur fase 3: tabel di atas 1MB, coba BIOS move-block --- */
        {
            static unsigned char move_buf[SMBIOS_MOVE_BUF_SIZE];
            unsigned copy_len = table_len;

            if (copy_len > SMBIOS_MOVE_BUF_SIZE) {
                copy_len = SMBIOS_MOVE_BUF_SIZE; /* dipotong -- jarang terjadi,
                                                   * tabel SMBIOS umum jauh
                                                   * lebih kecil dari 2KB */
            }

            if (bios_move_block(table_addr, (void far *)move_buf, copy_len)) {
                parse_smbios_table((unsigned char far *)move_buf, copy_len, out);
            } else {
                /* Wajar terjadi di CPU 8086/80186 (INT15h fungsi 87h
                 * memang belum ada di era itu) atau BIOS yang tak
                 * mengimplementasikan fungsi ini. */
                strcpy(out->vendor_name.text, "N/A (tabel >1MB, move-block gagal/tak didukung)");
                out->vendor_name.status = STATUS_UNSUPPORTED;
            }
        }
    }
}
