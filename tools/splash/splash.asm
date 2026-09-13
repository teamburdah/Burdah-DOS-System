; ============================================================
; SPLASH.COM - Burdah DOS boot splash utility (Rencana A)
;
; Menampilkan gambar 320x200, 256 warna (mode 13h) dari data
; yang di-embed langsung ke dalam file .COM ini (lewat incbin),
; lalu menunggu keypress ATAU timeout, kemudian kembali ke mode
; teks dan keluar.
;
; Dipanggil dari baris pertama BBAUTO.BAT.
;
; Semua lewat panggilan BIOS standar (INT 10h / INT 16h / INT 1Ah)
; -- tidak ada akses register VGA langsung, supaya aman dijalankan
; di 80286 asli, PCem, maupun QEMU tanpa perilaku berbeda-beda.
; ============================================================

                cpu     8086
                org     100h

TIMEOUT_TICKS   equ     91      ; ~5 detik (18.2 tick/detik BIOS)

start:
                ; --- simpan mode video lama (untuk kembali nanti) ---
                mov     ah, 0Fh
                int     10h             ; AL = mode video saat ini
                mov     [old_mode], al

                ; --- masuk mode 13h: 320x200, 256 warna ---
                mov     ax, 0013h
                int     10h

                ; --- load palet 256 warna (6-bit VGA DAC) ---
                mov     ax, 1012h
                mov     bx, 0           ; mulai dari warna index 0
                mov     cx, 256         ; jumlah warna
                mov     dx, cs
                mov     es, dx
                mov     dx, palette_data
                int     10h

                ; --- blit data pixel mentah ke A000:0000 ---
                push    cs
                pop     ds
                mov     si, pixel_data
                mov     ax, 0A000h
                mov     es, ax
                xor     di, di
                mov     cx, 64000       ; 320*200 byte, muat di satu segmen
                cld
                rep     movsb

                ; --- reset system tick counter (INT 1Ah AH=00h) ---
                xor     ax, ax
                int     1ah
                mov     [start_lo], dx
                mov     [start_hi], cx

wait_loop:
                ; keluar kalau ada tombol ditekan
                mov     ah, 01h
                int     16h
                jnz     key_pressed

                ; cek elapsed tick (abaikan kasus lewat tengah malam,
                ; cukup untuk splash beberapa detik saja)
                xor     ax, ax
                int     1ah
                sub     dx, [start_lo]
                cmp     dx, TIMEOUT_TICKS
                jb      wait_loop

key_pressed:
                ; kalau ada tombol yang menunggu di buffer, buang
                mov     ah, 01h
                int     16h
                jz      no_flush
                mov     ah, 00h
                int     16h
no_flush:

                ; --- kembali ke mode teks semula ---
                xor     ah, ah
                mov     al, [old_mode]
                int     10h

                ; --- keluar ke DOS ---
                mov     ax, 4C00h
                int     21h

; --------------------------------------------------------------
old_mode        db      0
start_lo        dw      0
start_hi        dw      0

; Data gambar di-embed langsung di sini. Ganti isi logo.raw /
; logo.pal untuk memakai logo final tanpa mengubah kode di atas.
;   logo.raw : 320*200 = 64000 byte, 1 byte/pixel, index palet
;   logo.pal : 768 byte (256 warna x 3), tiap komponen 0-63 (VGA DAC)
pixel_data:
                incbin  "logo.raw"
palette_data:
                incbin  "logo.pal"
