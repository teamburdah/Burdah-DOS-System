/* ===================================================================
 * detect_disk.c
 * Deteksi ukuran HDD pertama via BIOS INT 13h, daftar partisi primer
 * via parsing MBR, dan (FASE 2) nama model drive fisik via command
 * ATA IDENTIFY DEVICE yang dikirim LANGSUNG ke port I/O controller
 * IDE -- di luar cakupan BIOS interrupt biasa.
 *
 * CATATAN PENTING soal asumsi mapping drive:
 * BIOS drive 0x80 (HDD pertama menurut INT 13h) KONVENSIONALNYA
 * adalah primary master pada controller IDE standar (port base
 * 0x1F0-0x1F7). Ini asumsi umum yang berlaku di kebanyakan konfigurasi
 * standar (termasuk default 86Box), TAPI tidak selalu benar 100% kalau
 * boot order di-custom di BIOS/CMOS. Kalau primary master tidak
 * merespons, kode ini TIDAK otomatis mencoba slave/secondary --
 * ditandai TODO fase 3 kalau perlu pencarian lebih lengkap.
 * =================================================================== */

#include "detect_disk.h"
#include <dos.h>
#include <string.h>
#include <stdio.h>

#pragma pack(push, 1)
typedef struct {
    unsigned char boot_flag;
    unsigned char start_chs[3];
    unsigned char type;
    unsigned char end_chs[3];
    unsigned long start_lba;
    unsigned long size_sectors;
} mbr_entry_t;
#pragma pack(pop)

/* ===================================================================
 * Bagian ATA IDENTIFY DEVICE (akses port I/O langsung)
 * =================================================================== */

#define ATA_PRIMARY_BASE   0x1F0

#define ATA_REG_DATA       0
#define ATA_REG_SECCOUNT   2
#define ATA_REG_LBA_LO     3
#define ATA_REG_LBA_MID    4
#define ATA_REG_LBA_HI     5
#define ATA_REG_DRIVEHEAD  6
#define ATA_REG_STATUS     7
#define ATA_REG_COMMAND    7

#define ATA_CMD_IDENTIFY   0xEC

#define ATA_SR_BSY  0x80
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01

/* Batas iterasi polling supaya TIDAK PERNAH hang tanpa batas kalau
 * hardware tidak merespons -- sejalan dengan prinsip "tidak boleh
 * force-close/hang" yang disepakati di awal proyek. */
#define ATA_TIMEOUT_LOOPS 200000UL

static unsigned char ata_in8(unsigned base, unsigned offset)
{
    unsigned port = base + offset;
    unsigned char val;
    _asm {
        mov dx, port
        in  al, dx
        mov val, al
    }
    return val;
}

static void ata_out8(unsigned base, unsigned offset, unsigned char value)
{
    unsigned port = base + offset;
    _asm {
        mov dx, port
        mov al, value
        out dx, al
    }
}

static unsigned ata_in16(unsigned base)
{
    unsigned port = base + ATA_REG_DATA;
    unsigned val;
    _asm {
        mov dx, port
        in  ax, dx
        mov val, ax
    }
    return val;
}

static int ata_wait_not_busy(unsigned base)
{
    unsigned long i;
    for (i = 0; i < ATA_TIMEOUT_LOOPS; i++) {
        if (!(ata_in8(base, ATA_REG_STATUS) & ATA_SR_BSY)) return 1;
    }
    return 0; /* timeout */
}

/* Return: 1 = data siap (DRQ set), 0 = timeout, -1 = device lapor ERR
 * (kemungkinan ATAPI/CD-ROM, bukan hard disk ATA biasa). */
static int ata_wait_drq(unsigned base)
{
    unsigned long i;
    for (i = 0; i < ATA_TIMEOUT_LOOPS; i++) {
        unsigned char st = ata_in8(base, ATA_REG_STATUS);
        if (st & ATA_SR_ERR) return -1;
        if (st & ATA_SR_DRQ) return 1;
    }
    return 0;
}

/* Kirim command IDENTIFY DEVICE (0xECh) ke drive master/slave pada
 * base port yg diberikan.
 * Return: 1 = berhasil, buf (256 word / 512 byte) terisi data IDENTIFY
 *         0 = gagal/timeout/drive tak ada
 *        -1 = device menjawab ERR (kemungkinan ATAPI, bukan HDD ATA) */
static int ata_identify(unsigned base, int is_slave, unsigned buf[256])
{
    int i, r;
    unsigned char drive_sel = is_slave ? 0xB0 : 0xA0;

    if (!ata_wait_not_busy(base)) return 0;

    ata_out8(base, ATA_REG_DRIVEHEAD, drive_sel);

    /* Delay singkat -- baca status register beberapa kali sbg delay
     * konvensional (~400ns per baca) sebelum drive select settle. */
    for (i = 0; i < 4; i++) (void)ata_in8(base, ATA_REG_STATUS);

    ata_out8(base, ATA_REG_SECCOUNT, 0);
    ata_out8(base, ATA_REG_LBA_LO, 0);
    ata_out8(base, ATA_REG_LBA_MID, 0);
    ata_out8(base, ATA_REG_LBA_HI, 0);
    ata_out8(base, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    if (ata_in8(base, ATA_REG_STATUS) == 0) return 0; /* drive tidak ada sama sekali */

    if (!ata_wait_not_busy(base)) return 0;

    r = ata_wait_drq(base);
    if (r <= 0) return r;

    for (i = 0; i < 256; i++) {
        buf[i] = ata_in16(base);
    }
    return 1;
}

/* Ekstrak string dari data IDENTIFY (mis. model, firmware, serial).
 * Data ATA string disimpan byte-swapped per word (byte tinggi word
 * duluan), dan di-pad spasi di kanan -- keduanya ditangani di sini. */
static void ata_extract_string(const unsigned *identify_buf, int word_start,
                                int word_count, char *out, int out_size)
{
    int i, pos = 0;
    for (i = 0; i < word_count && pos < out_size - 2; i++) {
        unsigned w = identify_buf[word_start + i];
        out[pos++] = (char)(w >> 8);
        out[pos++] = (char)(w & 0xFF);
    }
    out[pos] = '\0';

    while (pos > 0 && out[pos - 1] == ' ') {
        out[--pos] = '\0';
    }
}

/* ===================================================================
 * Bagian MBR & geometri (via BIOS, tidak ada perubahan besar dari
 * fase 1 -- lihat detect_disk.h untuk gambaran umum modul ini)
 * =================================================================== */

static const char *partition_type_name(unsigned char type)
{
    switch (type) {
        case 0x00: return NULL; /* slot kosong, bukan partisi */
        case 0x01: return "FAT12";
        case 0x04:
        case 0x06:
        case 0x0E: return "FAT16";
        case 0x0B:
        case 0x0C: return "FAT32";
        case 0x05:
        case 0x0F: return "Extended";
        case 0x07: return "NTFS/HPFS";
        case 0x83: return "Linux";
        case 0x82: return "Linux Swap";
        default:   return "RAW";
    }
}

void detect_disk(disk_info_t *out)
{
    unsigned char buf[512];
    unsigned char drive = 0x80; /* HDD pertama menurut konvensi BIOS */
    int i;

    memset(out, 0, sizeof(*out));

    /* --- Nama drive fisik via ATA IDENTIFY DEVICE (FASE 2) --- */
    {
        static unsigned identify_buf[256]; /* static: hindari 512 byte di stack */
        int r = ata_identify(ATA_PRIMARY_BASE, 0 /* master */, identify_buf);

        if (r == 1) {
            char model[41];
            ata_extract_string(identify_buf, 27, 20, model, sizeof(model));
            if (model[0] != '\0') {
                strncpy(out->drive_name.text, model, sizeof(out->drive_name.text) - 1);
                out->drive_name.text[sizeof(out->drive_name.text) - 1] = '\0';
                out->drive_name.status = STATUS_OK;
            } else {
                strcpy(out->drive_name.text, "N/A (string model empty)");
                out->drive_name.status = STATUS_FAIL;
            }
        } else if (r == -1) {
            strcpy(out->drive_name.text, "N/A (maybe ATAPI, not HDD)");
            out->drive_name.status = STATUS_NA;
        } else {
            strcpy(out->drive_name.text, "N/A (timeout/not respond)");
            out->drive_name.status = STATUS_FAIL;
        }
    }

    /* --- Ukuran total drive via INT 13h AH=08h (get drive parameters) --- */
    {
        unsigned char heads = 0, sectors = 0, cf = 1;
        unsigned char cyl_hi = 0, cyl_lo = 0;

        _asm {
            mov ah, 08h
            mov dl, drive
            int 13h
            jc  fail08
            mov heads, dh
            mov al, cl
            and al, 3Fh
            mov sectors, al
            mov al, cl
            mov cl, 6
            shr al, cl
            mov cyl_hi, al
            mov cyl_lo, ch
            mov cf, 0
        fail08:
        }

        if (!cf && sectors > 0 && heads > 0) {
            unsigned long cyl = ((unsigned long)cyl_hi << 8) | cyl_lo;
            unsigned long total_sectors =
                (cyl + 1) * (unsigned long)heads * (unsigned long)sectors;
            unsigned long size_mb = (total_sectors * 512UL) / (1024UL * 1024UL);
            sprintf(out->total_size.text, "%lu MB", size_mb);
            out->total_size.status = STATUS_OK;
        } else {
            strcpy(out->total_size.text, "N/A");
            out->total_size.status = STATUS_NOT_DETECTED; /* HDD tak terpasang/tak terbaca */
        }
    }

    /* --- Baca MBR (sektor 0) untuk daftar partisi primer --- */
    {
        unsigned char cf = 1;
        void far *buf_ptr = (void far *)buf;

        _asm {
            mov ax, 0201h        ; AH=02h (read sector), AL=1 sektor
            mov cx, 0001h        ; cylinder 0, sector 1 (1-based)
            mov dh, 0            ; head 0
            mov dl, drive
            les bx, buf_ptr      ; ES:BX = alamat buffer
            int 13h
            jc  fail_mbr
            mov cf, 0
        fail_mbr:
        }

        out->partition_count = 0;

        if (!cf && buf[510] == 0x55 && buf[511] == 0xAA) {
            mbr_entry_t *entries = (mbr_entry_t *)(buf + 446);
            for (i = 0; i < 4 && out->partition_count < MAX_PARTITIONS; i++) {
                const char *tname = partition_type_name(entries[i].type);
                if (tname == NULL) continue;

                out->partitions[out->partition_count].valid = 1;
                strncpy(out->partitions[out->partition_count].type_name, tname, 15);
                out->partitions[out->partition_count].size_mb =
                    (entries[i].size_sectors * 512UL) / (1024UL * 1024UL);
                out->partition_count++;
            }
        }
    }
}
