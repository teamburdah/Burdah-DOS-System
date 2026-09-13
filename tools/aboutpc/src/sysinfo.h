#ifndef SYSINFO_H
#define SYSINFO_H

/* ===================================================================
 * sysinfo.h
 * Struktur data bersama untuk seluruh modul deteksi hardware.
 *
 * Project : dos-aboutpc
 * Target  : FreeDOS 1.4 / MS-DOS, kompatibel dari Intel 8086 ke atas
 * =================================================================== */

/* Kode status hasil deteksi tiap field. Dipakai render.c untuk
 * menentukan warna teks (hijau = OK, merah = selain OK). */
typedef enum {
    STATUS_OK = 0,        /* data berhasil dibaca                      */
    STATUS_NA,             /* fitur memang tidak ada di hardware ini    */
    STATUS_UNSUPPORTED,    /* BIOS/hardware terlalu lama utk fitur ini  */
    STATUS_FAIL,            /* command dikirim tapi gagal/timeout        */
    STATUS_NOT_DETECTED    /* device tidak ditemukan sama sekali        */
} status_t;

/* Satu field info + status-nya. Dipakai berulang di seluruh struct
 * di bawah -- pola ini yang bikin render.c bisa generik. */
typedef struct {
    status_t status;
    char     text[48];
} field_t;

#define MAX_PARTITIONS 4   /* MBR klasik maksimum 4 partisi primer */

typedef struct {
    int           valid;         /* 1 jika entry ini terisi           */
    char          type_name[16]; /* "FAT16","FAT32","RAW", dst        */
    unsigned long size_mb;       /* ukuran partisi dalam MB           */
} partition_t;

typedef struct {
    field_t vendor;      /* "GenuineIntel" dsb, hanya jika CPUID ada        */
    field_t name;        /* "Intel Pentium" / "Intel 80286 atau kompatibel" */
    field_t clock;       /* "100 MHz" atau status FAIL kalau belum terukur  */
    int     has_cpuid;   /* 1 jika CPU mendukung instruksi CPUID            */
    int     generation;  /* 86,186,286,386,486,586 (angka generik)          */
} cpu_info_t;

typedef struct {
    field_t conventional; /* "640 KB" */
    field_t extended;     /* "15 MB" atau N/A kalau tak ada extended memory */
} mem_info_t;

typedef struct {
    field_t     drive_name;   /* "HITACHI DK223A" atau N/A */
    field_t     total_size;   /* "450 MB" */
    partition_t partitions[MAX_PARTITIONS];
    int         partition_count;
} disk_info_t;

typedef struct {
    field_t vendor_name; /* "ASUS P/I-P55T2P4" dari SMBIOS, atau N/A */
} board_info_t;

typedef struct {
    field_t adapter;     /* "VGA","EGA","CGA","MDA" */
    field_t resolution;  /* "800 x 600" via VESA/EDID, atau kosong+status */
} video_info_t;

typedef struct {
    field_t      os_name;  /* "FreeDOS 1.4" dari OEM/versi DOS */
    cpu_info_t   cpu;
    mem_info_t   mem;
    board_info_t board;
    disk_info_t  disk;
    video_info_t video;
} sysinfo_t;

#endif /* SYSINFO_H */
