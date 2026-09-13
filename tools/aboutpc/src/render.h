#ifndef RENDER_H
#define RENDER_H

#include "sysinfo.h"

/* Bersihkan layar TANPA bergantung command shell eksternal (tidak
 * pakai system("cls")) -- baca mode video aktif lalu reset ke mode
 * yang sama via BIOS INT 10h. Bekerja di MDA/CGA/EGA/VGA. */
void clear_screen(void);

/* Render satu baris "LABEL : value", label warna default, value
 * warna sesuai field->status (hijau jika OK, merah selainnya).
 * `indent` = jumlah spasi tambahan sebelum label (dipakai untuk
 * baris PARTITION yang di-indent di bawah DISK). */
void render_field(const char *label, const field_t *field, int indent);

/* Render baris blok warna-warni (CP437 219 / 0xDB) sepanjang `width`
 * karakter, siklus 15 warna foreground standar teks DOS. */
void render_rainbow_bar(int width);

#endif /* RENDER_H */
