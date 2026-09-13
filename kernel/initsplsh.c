/*
 * initsplsh.c - Burdah DOS Boot Splash Loader (VGA Mode 13h 320x200 8bpp BMP)
 */

#include "portab.h"
#include "init-mod.h"

#ifdef __WATCOMC__
#pragma pack(__push, 1);
#else
#pragma pack(1)
#endif

typedef struct {
  UWORD bfType;         /* Magic: 'BM' = 0x4D42 */
  ULONG bfSize;
  UWORD bfReserved1;
  UWORD bfReserved2;
  ULONG bfOffBits;
} BMP_FILE_HDR;

typedef struct {
  ULONG biSize;         /* Size of header (40) */
  LONG  biWidth;        /* 320 */
  LONG  biHeight;       /* 200 (bottom-up) or -200 (top-down) */
  UWORD biPlanes;       /* 1 */
  UWORD biBitCount;     /* 8 */
  ULONG biCompression;  /* 0 = BI_RGB (uncompressed) */
  ULONG biSizeImage;
  LONG  biXPelsPerMeter;
  LONG  biYPelsPerMeter;
  ULONG biClrUsed;
  ULONG biClrImportant;
} BMP_INFO_HDR;

#ifdef __WATCOMC__
#pragma pack(__pop);
#else
#pragma pack()
#endif

/* Static buffers placed in INIT segment (freed after boot) */
STATIC BYTE row_buf[320] BSS_INIT({0});
STATIC BYTE pal_buf[1024] BSS_INIT({0});
STATIC BYTE dac_pal[768] BSS_INIT({0});

STATIC void set_video_mode(UBYTE mode)
{
  iregs r;
  r.a.b.h = 0x00;
  r.a.b.l = mode;
  init_call_intr(0x10, &r);
}

int splash_init(void)
{
  BMP_FILE_HDR file_hdr;
  BMP_INFO_HDR info_hdr;
  int fd = -1;
  int i;
  COUNT row;
  int is_top_down = 0;
  char filename[32];
  char boot_drive;
  int candidate;
  iregs r;

  splash_active = 0;

  boot_drive = (char)('A' + LoL->BootDrive - 1);

  /* Coba tiap kandidat berurutan. PENTING: lanjut ke kandidat berikut
     bukan hanya kalau open() gagal, tapi juga kalau validasi header BMP
     gagal -- karena <BootDrive>:\BURDAH.SYS SELALU ada (itu file kernel
     itu sendiri) dan open()-nya akan selalu sukses, jadi tanpa fallback
     berbasis validasi ini LOGO.SYS tidak akan pernah dicoba sama sekali. */
  for (candidate = 0; candidate < 4; candidate++)
  {
    fd = -1;
    switch (candidate)
    {
      case 0: /* <BootDrive>:\LOGO.SYS -- prioritas utama */
        filename[0] = boot_drive;
        filename[1] = ':';
        filename[2] = '\\';
        strcpy(filename + 3, "LOGO.SYS");
        fd = open(filename, O_RDONLY);
        break;
      case 1: /* <BootDrive>:\BURDAH.SYS -- hanya relevan kalau kernel
                 suatu saat tidak lagi memakai nama file ini */
        filename[0] = boot_drive;
        filename[1] = ':';
        filename[2] = '\\';
        strcpy(filename + 3, "BURDAH.SYS");
        fd = open(filename, O_RDONLY);
        break;
      case 2: /* fallback path absolut, drive lain/pemetaan tak biasa */
        fd = open("\\LOGO.SYS", O_RDONLY);
        break;
      case 3:
        fd = open("\\BURDAH.SYS", O_RDONLY);
        break;
    }

    if (fd < 0)
      continue;

    /* Baca & validasi BMP file header */
    if (read(fd, &file_hdr, sizeof(file_hdr)) != sizeof(file_hdr)) {
      close(fd);
      continue;
    }

    /* Verifikasi signature 'BM' */
    if (file_hdr.bfType != 0x4D42) {
      close(fd);
      continue;
    }

    /* Baca BMP info header */
    if (read(fd, &info_hdr, sizeof(info_hdr)) != sizeof(info_hdr)) {
      close(fd);
      continue;
    }

    /* Validasi 320x200 8-bit uncompressed */
    if (info_hdr.biWidth != 320 ||
        (info_hdr.biHeight != 200 && info_hdr.biHeight != -200) ||
        info_hdr.biBitCount != 8 ||
        info_hdr.biCompression != 0) {
      close(fd);
      continue;
    }

    /* Kandidat ini valid -- pakai */
    break;
  }

  if (candidate >= 4)
    return 0; /* Tidak ada gambar splash yang valid ditemukan */

  if (info_hdr.biHeight < 0)
    is_top_down = 1;

  /* Seek to palette */
  if (lseek(fd, 14 + info_hdr.biSize) == 0xffffffffL) {
    close(fd);
    return 0;
  }

  /* Read palette (256 entries * 4 bytes RGBQUAD: B, G, R, 0) */
  if (read(fd, pal_buf, 1024) != 1024) {
    close(fd);
    return 0;
  }

  /* Convert 8-bit RGB (0..255) to VGA 6-bit DAC values (0..63) */
  for (i = 0; i < 256; i++) {
    dac_pal[i * 3 + 0] = (BYTE)(pal_buf[i * 4 + 2] >> 2); /* Red */
    dac_pal[i * 3 + 1] = (BYTE)(pal_buf[i * 4 + 1] >> 2); /* Green */
    dac_pal[i * 3 + 2] = (BYTE)(pal_buf[i * 4 + 0] >> 2); /* Blue */
  }

  /* Seek to pixel data */
  if (lseek(fd, file_hdr.bfOffBits) == 0xffffffffL) {
    close(fd);
    return 0;
  }

  /* Switch video mode to 320x200 256-color (Mode 13h) */
  set_video_mode(0x13);

  /* Set VGA palette via BIOS INT 10h AX=1012h */
  r.a.x = 0x1012;
  r.b.x = 0;
  r.c.x = 256;
  r.d.x = FP_OFF((void FAR *)dac_pal);
  r.es  = FP_SEG((void FAR *)dac_pal);
  init_call_intr(0x10, &r);

  /* Blit pixel rows directly to VGA video segment 0xA000:0000 */
  for (row = 0; row < 200; row++) {
    COUNT y;
    if (read(fd, row_buf, 320) != 320)
      break;

    if (is_top_down)
      y = row;
    else
      y = 199 - row;

    fmemcpy(MK_FP(0xA000, (unsigned)y * 320), (void FAR *)row_buf, 320);
  }

  close(fd);

  /* Enable quiet boot mode / suppress int 29h console text */
  splash_active = 1;

  return 1;
}

void splash_check_abort(void)
{
  iregs r;
  if (!splash_active)
    return;

  /* BIOS INT 16h AH=01h: check keystroke buffer */
  r.a.x = 0x0100;
  init_call_intr(0x16, &r);
  if (!(r.flags & FLG_ZERO)) {
    /* Keystroke waiting - fetch it */
    r.a.x = 0x0000;
    init_call_intr(0x16, &r);

    /* If ESC (ASCII 0x1B) or any key is pressed, dismiss splash */
    splash_close();
  }
}

void splash_close(void)
{
  if (splash_active) {
    splash_active = 0;
    /* Restore standard 80x25 Color Text Mode (Mode 03h) */
    set_video_mode(0x03);
  }
}
