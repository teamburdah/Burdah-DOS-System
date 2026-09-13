/* ===================================================================
 * render.c
 * Output teks berwarna ke layar DOS via BIOS INT 10h -- tidak
 * bergantung command shell eksternal, tidak butuh conio.h/textcolor()
 * ala Turbo C (yang tidak portable ke semua compiler DOS).
 *
 * Skema warna (attribute byte standar CGA/VGA text mode):
 *   0x07 = light gray  -> label field (default)
 *   0x0A = light green -> data valid (status OK)
 *   0x0C = light red   -> data error/N/A (status selain OK)
 * =================================================================== */

#include "render.h"
#include <dos.h>
#include <string.h>

#define COL_DEFAULT 0x07   /* light gray - label */
#define COL_GOOD    0x0A   /* light green - data valid */
#define COL_BAD     0x0C   /* light red - data error/NA */

/* Tulis satu karakter dengan attribute warna tertentu di posisi
 * cursor saat ini, lalu majukan cursor satu kolom. INT 10h AH=09h
 * ("write char + attribute") TIDAK menggerakkan cursor otomatis --
 * jadi kita majukan manual via AH=02h (set cursor position) setelah
 * membaca posisi saat ini via AH=03h. Ini lebih portabel ketimbang
 * menulis langsung ke memori video B800:0000, karena BIOS yang
 * menentukan alamat basis video (beda antara MDA/CGA/EGA/VGA). */
static void put_char_color(char ch, unsigned char attr)
{
    unsigned char row = 0, col = 0;

    _asm {
        mov ah, 09h
        mov al, ch
        mov bl, attr
        mov bh, 0        ; video page 0
        mov cx, 1        ; tulis 1 karakter
        int 10h
    }

    _asm {
        mov ah, 03h      ; get cursor position
        mov bh, 0
        int 10h
        mov row, dh
        mov col, dl
    }

    if (col == 79) {
        /* Wrap manual ke baris berikutnya kalau sudah di kolom
         * terakhir, supaya tidak keluar batas layar 80 kolom. */
        col = 0;
        row++;
    } else {
        col++;
    }

    _asm {
        mov ah, 02h      ; set cursor position
        mov bh, 0
        mov dh, row
        mov dl, col
        int 10h
    }
}

static void put_string_color(const char *s, unsigned char attr)
{
    while (*s) {
        put_char_color(*s, attr);
        s++;
    }
}

static void put_string_plain(const char *s)
{
    /* Untuk teks non-warna (spasi, separator ": ", newline), pakai
     * teletype biasa (AH=0Eh) yang otomatis menangani \r\n & scroll,
     * lebih sederhana daripada put_char_color untuk kasus ini. */
    while (*s) {
        _asm {
            mov ah, 0Eh
            mov al, byte ptr [s]
            mov bh, 0
            int 10h
        }
        s++;
    }
}

static unsigned char color_for_status(status_t st)
{
    return (st == STATUS_OK) ? COL_GOOD : COL_BAD;
}

void render_field(const char *label, const field_t *field, int indent)
{
    int i;
    for (i = 0; i < indent; i++) put_string_plain(" ");

    put_string_color(label, COL_DEFAULT);
    put_string_plain(" : ");
    put_string_color(field->text, color_for_status(field->status));
    put_string_plain("\r\n");
}

void render_rainbow_bar(int width)
{
    /* 15 warna foreground standar (melewati 0x00/hitam karena tak
     * kelihatan di background hitam default). */
    static const unsigned char colors[] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
    };
    int i;
    for (i = 0; i < width; i++) {
        unsigned char attr = colors[i % (sizeof(colors) / sizeof(colors[0]))];
        put_char_color((char)0xDB, attr); /* CP437 219 = blok penuh */
    }
    put_string_plain("\r\n");
}

void clear_screen(void)
{
    unsigned char mode = 0;

    /* Baca mode video aktif dulu, lalu reset ke mode YANG SAMA.
     * Cara ini portable ke MDA/CGA/EGA/VGA -- tidak mengasumsikan
     * mode 3 (80x25 warna) yang mungkin tak ada di adapter mono. */
    _asm {
        mov ah, 0Fh
        int 10h
        mov mode, al
    }
    _asm {
        mov ah, 00h
        mov al, mode
        int 10h
    }
}
