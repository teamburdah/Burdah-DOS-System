/* ===================================================================
 * detect_cpu.c
 *
 * Compatibility gate: deteksi generasi CPU x86 dari 8086 s.d. CPU
 * bermdukung CPUID (Pentium generation ke atas), TANPA bergantung
 * instruksi apapun yang mungkin tak ada di CPU tua -- semua tahap
 * awal cuma pakai instruksi yang sudah ada sejak 8086 (pushf/popf/
 * push/pop/and/cmp).
 *
 * Teknik ini klasik dan dipakai banyak utility DOS lawas (MSD, Norton
 * SysInfo, dsb). Prinsipnya: tiap generasi CPU x86 mengubah perilaku
 * bit-bit tertentu di FLAGS/EFLAGS register yang "tidak dipakai" oleh
 * generasi sebelumnya.
 *
 * FASE 2 menambahkan estimasi clock speed:
 *   - CPU dgn CPUID + TSC (Pentium+): pakai RDTSC, akurat.
 *   - CPU tanpa CPUID (8086-486): loop timing vs PIT tick, ESTIMASI
 *     kasar (+-10-20%) berdasar tabel siklus/iterasi per generasi.
 *     Tabel ini PERLU dikalibrasi ulang dengan membandingkan hasil di
 *     86Box terhadap clock speed yang di-set manual di konfigurasi
 *     mesin virtualnya (86Box mengizinkan set MHz presisi per profil
 *     CPU, jadi ini kesempatan validasi yang bagus).
 *
 * PENTING: kode ini ditulis untuk Open Watcom C inline assembler.
 * Instruksi 32-bit (pushfd/popfd/eax dst) hanya dieksekusi setelah
 * tahap sebelumnya memastikan CPU minimal 80386 -- jadi aman dijalankan
 * di real hardware 386+ meski source file di-compile target 8086 (-0),
 * karena percabangannya dilakukan di runtime, bukan compile-time.
 * =================================================================== */

#include "detect_cpu.h"
#include <string.h>
#include <stdio.h>
#include <dos.h>

/* Jendela pengukuran clock speed dalam satuan PIT tick (~18.2065
 * tick/detik). 9 tick ~ 0.5 detik -- kompromi antara akurasi dan
 * waktu tunggu startup aplikasi. */
#define CLOCK_MEASURE_TICKS 9UL

/* Mikrodetik per PIT tick, dibulatkan dari 65536/1193182*1e6.
 * Dipakai supaya semua perhitungan tetap integer math (hindari
 * floating point di DOS 16-bit -- lebih ringan & tak butuh FP
 * library/emulation). */
#define US_PER_TICK 54925UL

/* -------------------------------------------------------------------
 * flags_probe()
 * Return: generasi CPU generik -> 86 / 286 / 386 / 486 / 586
 *   586 berarti "CPU mendukung CPUID", bukan berarti benar-benar
 *   Pentium asli (Cyrix/AMD generasi setara juga akan lolos di sini,
 *   makanya nama presisi baru diambil dari CPUID vendor+brand string).
 * ------------------------------------------------------------------- */
static int flags_probe(void)
{
    unsigned test_hi, test_lo;
    int gen = 86; /* default asumsi paling tua kalau semua tes gagal lolos */

    /* --- Tahap 1: 8086/80188 vs 80286+ ---
     * Di 8086/80188, bit 12-15 FLAGS SELALU terbaca 1 walau dicoba
     * di-clear paksa. Di 80286 ke atas, bit-bit ini bisa di-clear. */
    _asm {
        xor ax, ax
        push ax
        popf
        pushf
        pop ax
        and ax, 0F000h
        mov test_lo, ax
    }
    if (test_lo == 0xF000) {
        return 86; /* 8086/80188 -- tidak bisa clear bit 12-15 */
    }

    /* --- Tahap 2: 80286 vs 80386+ ---
     * Di 80286 real mode, bit 12-15 tidak bisa di-SET ke 1.
     * Di 80386 ke atas, bit-bit ini bisa di-set ke 1. */
    _asm {
        mov ax, 0F000h
        push ax
        popf
        pushf
        pop ax
        and ax, 0F000h
        mov test_hi, ax
    }
    if (test_hi != 0xF000) {
        return 286;
    }

    /* Mulai titik ini CPU dipastikan minimal 80386 (bit 12-15
     * settable), jadi instruksi 32-bit (pushfd/popfd) aman dipakai. */

    /* --- Tahap 3: 80386 vs 80486+ ---
     * Uji AC flag (bit 18 EFLAGS / Alignment Check).
     * 80386 tidak bisa men-set bit ini; 80486 ke atas bisa. */
    {
        unsigned long f1 = 0, f2 = 0;
        _asm {
            pushfd
            pop eax
            mov f1, eax
            or eax, 00040000h      ; set bit 18 (AC)
            push eax
            popfd
            pushfd
            pop eax
            mov f2, eax
            popfd                  ; restore approksimasi (lihat catatan bawah)
        }
        if ((f1 & 0x00040000UL) == (f2 & 0x00040000UL)) {
            gen = 386;
            goto done;
        }
    }

    /* --- Tahap 4: 80486 vs Pentium+ (dukungan CPUID) ---
     * Uji ID flag (bit 21 EFLAGS). Kalau bisa di-toggle, CPU
     * mendukung instruksi CPUID. */
    {
        unsigned long f1 = 0, f2 = 0;
        _asm {
            pushfd
            pop eax
            mov f1, eax
            xor eax, 00200000h     ; toggle bit 21 (ID)
            push eax
            popfd
            pushfd
            pop eax
            mov f2, eax
        }
        if ((f1 & 0x00200000UL) == (f2 & 0x00200000UL)) {
            gen = 486;  /* tidak bisa toggle -> CPUID tidak ada */
        } else {
            gen = 586;  /* toggle berhasil -> CPUID tersedia */
        }
    }

done:
    /* Catatan implementasi: restore FLAGS asli di sini masih parsial
     * -- ditandai TODO fase 3 di PROGRESS.md, urutan pushfd/popfd yang
     * sepenuhnya bersih perlu diuji di real 386+ hardware/emulator
     * supaya tidak salah nesting stack. */
    return gen;
}

/* -------------------------------------------------------------------
 * read_cpuid_strings()
 * Hanya dipanggil kalau flags_probe() sudah memastikan CPUID ada.
 * Ambil vendor string (leaf 0) dan brand string (leaf 0x80000002-4,
 * kalau CPU cukup baru untuk mendukungnya).
 * ------------------------------------------------------------------- */
static void read_cpuid_strings(cpu_info_t *out)
{
    unsigned long r_ebx = 0, r_ecx = 0, r_edx = 0;
    unsigned long max_leaf = 0, max_ext_leaf = 0;
    char vendor[13];

    /* Leaf 0: vendor string 12 karakter, urutan EBX-EDX-ECX (bukan
     * EBX-ECX-EDX -- ini kekhasan encoding CPUID yang sering salah
     * diingat). */
    _asm {
        mov eax, 0
        cpuid
        mov max_leaf, eax
        mov r_ebx, ebx
        mov r_edx, edx
        mov r_ecx, ecx
    }
    memcpy(vendor,     &r_ebx, 4);
    memcpy(vendor + 4, &r_edx, 4);
    memcpy(vendor + 8, &r_ecx, 4);
    vendor[12] = '\0';

    strncpy(out->vendor.text, vendor, sizeof(out->vendor.text) - 1);
    out->vendor.text[sizeof(out->vendor.text) - 1] = '\0';
    out->vendor.status = STATUS_OK;

    /* Cek dukungan extended leaf untuk brand string (nama prosesor
     * lengkap seperti "Intel(R) Pentium(R) III"). Umumnya baru ada
     * di CPU generasi akhir 1990-an ke atas -- Pentium/Pentium Pro
     * awal biasanya TIDAK punya ini, jadi fallback ke nama generik
     * tetap penting. */
    _asm {
        mov eax, 80000000h
        cpuid
        mov max_ext_leaf, eax
    }

    if (max_ext_leaf >= 0x80000004UL) {
        char brand[49];
        unsigned long a, b, c, d;

        _asm {
            mov eax, 80000002h
            cpuid
            mov a, eax
            mov b, ebx
            mov c, ecx
            mov d, edx
        }
        memcpy(brand,      &a, 4);
        memcpy(brand + 4,  &b, 4);
        memcpy(brand + 8,  &c, 4);
        memcpy(brand + 12, &d, 4);

        _asm {
            mov eax, 80000003h
            cpuid
            mov a, eax
            mov b, ebx
            mov c, ecx
            mov d, edx
        }
        memcpy(brand + 16, &a, 4);
        memcpy(brand + 20, &b, 4);
        memcpy(brand + 24, &c, 4);
        memcpy(brand + 28, &d, 4);

        _asm {
            mov eax, 80000004h
            cpuid
            mov a, eax
            mov b, ebx
            mov c, ecx
            mov d, edx
        }
        memcpy(brand + 32, &a, 4);
        memcpy(brand + 36, &b, 4);
        memcpy(brand + 40, &c, 4);
        memcpy(brand + 44, &d, 4);
        brand[48] = '\0';

        strncpy(out->name.text, brand, sizeof(out->name.text) - 1);
        out->name.text[sizeof(out->name.text) - 1] = '\0';
        out->name.status = STATUS_OK;
    } else {
        sprintf(out->name.text, "%.30s (detail name N/A)", vendor);
        out->name.status = STATUS_NA; /* fitur brand string memang tak ada di CPU ini */
    }

    (void)max_leaf;
}

/* -------------------------------------------------------------------
 * cpu_has_tsc()
 * Cek bit TSC (bit 4) di EDX hasil CPUID leaf 1 (Feature Flags).
 * Hampir semua CPU dgn CPUID punya TSC, tapi tidak dijamin 100% utk
 * clone non-Intel generasi sangat awal -- jadi tetap dicek eksplisit
 * sebelum pakai RDTSC.
 * ------------------------------------------------------------------- */
static int cpu_has_tsc(void)
{
    unsigned long features_edx = 0;
    _asm {
        mov eax, 1
        cpuid
        mov features_edx, edx
    }
    return (features_edx & 0x00000010UL) ? 1 : 0; /* bit 4 = TSC */
}

/* -------------------------------------------------------------------
 * read_bios_tick()
 * Baca tick counter PIT via BIOS INT 1Ah AH=00h (CX:DX). Fungsi ini
 * ada sejak IBM PC BIOS pertama -- aman dipakai di 8086 murni.
 * ------------------------------------------------------------------- */
static unsigned long read_bios_tick(void)
{
    unsigned cx_val = 0, dx_val = 0;
    _asm {
        mov ah, 00h
        int 1Ah
        mov cx_val, cx
        mov dx_val, dx
    }
    return ((unsigned long)cx_val << 16) | dx_val;
}

/* -------------------------------------------------------------------
 * read_tsc_low()
 * Baca 32 bit rendah Time-Stamp Counter (RDTSC). Cukup untuk jendela
 * pengukuran ~0.5-1 detik di kisaran clock speed wajar (tidak wrap
 * dalam waktu sesingkat itu).
 * ------------------------------------------------------------------- */
static unsigned long read_tsc_low(void)
{
    unsigned long lo = 0;
    _asm {
        rdtsc
        mov lo, eax
    }
    return lo;
}

/* -------------------------------------------------------------------
 * measure_mhz_rdtsc()
 * Ukur MHz akurat via delta RDTSC selama jendela waktu tertentu
 * (diukur via PIT tick BIOS). CPU harus sudah dipastikan punya TSC.
 * ------------------------------------------------------------------- */
static unsigned long measure_mhz_rdtsc(unsigned long target_ticks)
{
    unsigned long start_tick, cur_tick;
    unsigned long tsc_start, tsc_end, tsc_delta;
    unsigned long elapsed_us;

    /* Sinkron ke awal tick baru dulu supaya error pengukuran maksimum
     * cuma 1 tick (~55ms), bukan bisa mulai di tengah tick. */
    start_tick = read_bios_tick();
    do {
        cur_tick = read_bios_tick();
    } while (cur_tick == start_tick);
    start_tick = cur_tick;

    tsc_start = read_tsc_low();

    do {
        cur_tick = read_bios_tick();
    } while ((cur_tick - start_tick) < target_ticks);

    tsc_end = read_tsc_low();
    tsc_delta = tsc_end - tsc_start; /* asumsi tak wrap dlm jendela ini */

    elapsed_us = target_ticks * US_PER_TICK;
    if (elapsed_us == 0) return 0;

    return tsc_delta / elapsed_us; /* cycles/us == MHz */
}

/* -------------------------------------------------------------------
 * run_calib_block() / cycles_per_iter_for_generation() /
 * measure_loop_iterations() / measure_mhz_loop()
 *
 * Metode fallback untuk CPU tanpa TSC (8086-486): jalankan blok loop
 * kalibrasi (DEC CX / JNZ, 65535 iterasi) berulang-ulang selama
 * jendela waktu tertentu, hitung total iterasi, lalu konversi ke MHz
 * pakai tabel referensi siklus-per-iterasi.
 *
 * Karena seluruh program di-compile target -0 (instruksi 8086 murni),
 * kode mesin blok kalibrasi ini SAMA PERSIS di semua generasi CPU --
 * yang beda cuma kecepatan eksekusinya (pipelining dsb di CPU baru).
 * Ini justru bikin pengukuran adil antar generasi, TAPI tetap butuh
 * tabel siklus/iterasi referensi karena siklus mesin per instruksi
 * (bukan cuma clock rate) juga beda antar generasi.
 * ------------------------------------------------------------------- */
#define CALIB_BLOCK_ITERS 65535UL

static unsigned long run_calib_block(void)
{
    _asm {
        push cx
        mov cx, 0FFFFh
    calib_block_loop:
        dec cx
        jnz calib_block_loop
        pop cx
    }
    return CALIB_BLOCK_ITERS;
}

/* Referensi siklus clock per iterasi loop kalibrasi (DEC CX/JNZ),
 * berdasarkan dokumentasi timing instruksi Intel klasik (taken branch
 * mendominasi krn cuma iterasi terakhir yg not-taken). Ini PENDEKATAN,
 * bukan angka pasti -- SILAKAN DIKALIBRASI ULANG dgn membandingkan
 * hasil di 86Box terhadap clock speed yg di-set manual di konfigurasi
 * mesin virtualnya (misal set 86Box ke CPU 80286 @ 12 MHz, jalankan
 * app ini, lihat berapa MHz yang dilaporkan, sesuaikan konstanta di
 * bawah kalau melenceng jauh). */
static unsigned cycles_per_iter_for_generation(int gen)
{
    switch (gen) {
        case 86:  return 18; /* 8086/8088: DEC reg=2, JNZ taken~16 */
        case 286: return 9;  /* 80286: DEC reg=2, JNZ taken~7 */
        case 386: return 8;  /* 80386: DEC reg=2, JNZ taken~7, sedikit overlap pipeline */
        case 486: return 4;  /* 80486: pipeline, DEC reg=1, JNZ taken~3 */
        default:  return 4;  /* fallback konservatif */
    }
}

static unsigned long measure_loop_iterations(unsigned long target_ticks)
{
    unsigned long start_tick, cur_tick;
    unsigned long total_iters = 0;

    start_tick = read_bios_tick();
    do {
        cur_tick = read_bios_tick();
    } while (cur_tick == start_tick);
    start_tick = cur_tick;

    do {
        total_iters += run_calib_block();
        cur_tick = read_bios_tick();
    } while ((cur_tick - start_tick) < target_ticks);

    return total_iters;
}

static unsigned long measure_mhz_loop(int gen, unsigned long target_ticks)
{
    unsigned long iters = measure_loop_iterations(target_ticks);
    unsigned cycles = cycles_per_iter_for_generation(gen);
    unsigned long total_cycles = iters * (unsigned long)cycles;
    unsigned long elapsed_us = target_ticks * US_PER_TICK;

    if (elapsed_us == 0) return 0;
    return total_cycles / elapsed_us;
}

void detect_cpu(cpu_info_t *out)
{
    int use_rdtsc;
    unsigned long mhz;

    memset(out, 0, sizeof(*out));

    out->generation = flags_probe();
    out->has_cpuid  = (out->generation >= 586);

    if (out->has_cpuid) {
        read_cpuid_strings(out);
    } else {
        const char *label;
        switch (out->generation) {
            case 86:  label = "Intel 8086/8088 or compatible model"; break;
            case 286: label = "Intel 80286 or compatible model";     break;
            case 386: label = "Intel 80386 or compatible model";     break;
            case 486: label = "Intel 80486 or compatible model";     break;
            default:  label = "Unknown CPU generation";     break;
        }
        strncpy(out->name.text, label, sizeof(out->name.text) - 1);
        out->name.status = STATUS_NA; /* memang tak ada CPUID di generasi ini */

        strcpy(out->vendor.text, "N/A");
        out->vendor.status = STATUS_NA;
    }

    use_rdtsc = out->has_cpuid && cpu_has_tsc();

    if (use_rdtsc) {
        mhz = measure_mhz_rdtsc(CLOCK_MEASURE_TICKS);
    } else {
        mhz = measure_mhz_loop(out->generation, CLOCK_MEASURE_TICKS);
    }

    if (mhz > 0) {
        if (use_rdtsc) {
            sprintf(out->clock.text, "%lu MHz", mhz);
        } else {
            sprintf(out->clock.text, "~%lu MHz (estimate)", mhz);
        }
        out->clock.status = STATUS_OK;
    } else {
        strcpy(out->clock.text, "N/A (measurement error)");
        out->clock.status = STATUS_FAIL;
    }
}
