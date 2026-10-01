/* ===================================================================
 * detect_cpu.c
 *
 * Compatibility gate: deteksi generasi CPU x86 dari 8086 s.d. CPU
 * bermdukung CPUID (Pentium generation ke atas).
 *
 * PENTING (temuan dari pengujian compile nyata dgn Open Watcom):
 * file ini HANYA berisi instruksi yang valid di target CPU 8086
 * (compile flag -0). Semua kode yang butuh instruksi 386+/CPUID/
 * RDTSC (pushfd/popfd, register 32-bit, cpuid, rdtsc) DIPINDAH ke
 * detect_cpu_x86.c yang dikompilasi terpisah dengan target CPU lebih
 * tinggi (-5) -- assembler Open Watcom MENOLAK instruksi di luar
 * target CPU yang dipilih saat compile. Lihat detect_cpu_x86.h dan
 * DECISIONS.md poin 9 untuk detail lengkap & alasannya.
 *
 * PENTING #2 (temuan lapangan, uji lintas mesin 86Box + QEMU):
 * Tahap "286 vs 386+" dari compatibility gate TADINYA pakai kuirk
 * FLAGS bit 12-15 (coba SET bit IOPL/NT via POPF 16-bit, cek apakah
 * "nempel"). Teknik ini KLASIK dan seharusnya bekerja di hardware
 * ASLI, tapi TERBUKTI TIDAK DAPAT DIANDALKAN di banyak lingkungan
 * virtualisasi -- dikonfirmasi user: 86Box (lintas beberapa profil
 * mesin) dan QEMU KEDUANYA salah menyimpulkan CPU sbg 80286 padahal
 * CPU sebenarnya jauh lebih baru (bahkan ~1GHz-class di QEMU). Dugaan
 * kuat: hypervisor memvirtualisasi/membatasi manipulasi IOPL di real
 * mode (flag terkait privilege I/O, target wajar dimediasi hypervisor
 * demi keamanan/isolasi).
 *
 * PERBAIKAN: tahap ini diganti dari "tes kuirk nilai FLAGS" menjadi
 * "coba EKSEKUSI instruksi 386+ sungguhan (pushfd), tangkap fault
 * dengan aman kalau CPU tak mendukungnya" via setjmp/longjmp + handler
 * INT 6 (invalid opcode). Pendekatan ini jauh lebih andal karena
 * menguji KAPABILITAS EKSEKUSI INSTRUKSI NYATA -- sesuatu yang WAJIB
 * diemulasikan benar oleh emulator/hypervisor manapun supaya software
 * lain (yg jauh lebih umum drpd utility deteksi CPU jadul) bisa
 * berjalan -- BUKAN kuirk nilai flag yang jauh lebih niche & rentan
 * "disaring" oleh lapisan virtualisasi.
 * =================================================================== */

#include "detect_cpu.h"
#include "detect_cpu_x86.h"
#include <string.h>
#include <stdio.h>
#include <dos.h>
#include <setjmp.h>

/* Jendela pengukuran clock speed dalam satuan PIT tick (~18.2065
 * tick/detik). 9 tick ~ 0.5 detik -- kompromi antara akurasi dan
 * waktu tunggu startup aplikasi. */
#define CLOCK_MEASURE_TICKS 9UL

/* Mikrodetik per PIT tick, dibulatkan dari 65536/1193182*1e6. Dipakai
 * supaya semua perhitungan tetap integer math (hindari floating
 * point di DOS 16-bit). CATATAN: konstanta yang sama juga didefinisi
 * ulang di detect_cpu_x86.c -- sengaja, karena kedua file dikompilasi
 * dengan target CPU berbeda dan sebaiknya tetap mandiri satu sama
 * lain (lihat detect_cpu_x86.h). */
#define US_PER_TICK 54925UL

/* Nilai sentinel: flags_probe_basic() mengembalikan ini kalau CPU
 * dipastikan >=80386 dan butuh probing lanjutan di modul terpisah. */
#define GEN_NEEDS_EXTENDED_PROBE (-1)

/* -------------------------------------------------------------------
 * cpu_supports_386_instructions()
 *
 * Coba jalankan pushfd/popfd (instruksi 386+ murni, via probe_pushfd_raw
 * di detect_cpu_x86.c yang dikompilasi target -5) DILINDUNGI setjmp/
 * longjmp + handler INT 6 (invalid opcode).
 *
 * Kalau CPU sungguhan (8086/80186/80286) tak mengenal instruksi itu,
 * CPU akan memicu exception INT 6 SEBELUM instruksi sempat mengubah
 * apapun (fault terjadi di tahap decode). Handler kita menangkap ini
 * dengan longjmp() -- teknik "longjmp manual" klasik utk mencoba
 * instruksi yang MUNGKIN tak didukung TANPA risiko hang/crash. INT 6
 * adalah CPU EXCEPTION murni (bukan hardware IRQ), jadi tidak perlu
 * kirim EOI ke PIC 8259 setelah menanganinya.
 *
 * Return: 1 jika CPU mendukung (pushfd/popfd sukses tanpa fault),
 *         0 jika tidak (fault tertangkap dgn aman).
 * ------------------------------------------------------------------- */
static jmp_buf g_fault_env;

static void __interrupt __far int6_handler(void)
{
    longjmp(g_fault_env, 1);
}

static int cpu_supports_386_instructions(void)
{
    void (__interrupt __far *old_int6)();
    int result;

    old_int6 = _dos_getvect(6);
    _dos_setvect(6, int6_handler);

    if (setjmp(g_fault_env) == 0) {
        probe_pushfd_raw();   /* dari detect_cpu_x86.c, target -5 */
        result = 1;            /* lolos tanpa fault -> CPU 386+ */
    } else {
        result = 0;            /* fault INT6 tertangkap -> CPU < 386 */
    }

    _dos_setvect(6, old_int6);
    return result;
}

/* -------------------------------------------------------------------
 * flags_probe_basic()
 * Tahap 1 (8086 vs 286+, kuirk FLAGS bit 12-15 -- INI TETAP DIPAKAI,
 * TIDAK bermasalah spt tahap 2 lama, karena bukan flag privilege
 * seperti IOPL/NT dan tidak terindikasi bermasalah dari laporan user)
 * + Tahap 2 (286 vs 386+, via cpu_supports_386_instructions() -- BARU).
 * Return: 86, 286, atau GEN_NEEDS_EXTENDED_PROBE (berarti >=80386,
 * generasi presisinya baru diketahui via flags_probe_extended()).
 * ------------------------------------------------------------------- */
static int flags_probe_basic(void)
{
    unsigned orig16 = 0;
    unsigned test_lo;

    _asm {
        pushf
        pop orig16
    }

    /* --- Tahap 1: 8086/80188 vs 80286+ ---
     * Di 8086/80188, bit 12-15 FLAGS SELALU terbaca 1 walau dicoba
     * di-clear paksa. Di 80286 ke atas, bit-bit ini bisa di-clear.
     * (Instruksi 16-bit biasa, tidak ada risiko fault sama sekali --
     * tidak perlu jaring pengaman setjmp/longjmp di tahap ini.) */
    _asm {
        xor ax, ax
        push ax
        popf
        pushf
        pop ax
        and ax, 0F000h
        mov test_lo, ax
    }

    _asm {
        push orig16
        popf
    }

    if (test_lo == 0xF000) {
        return 86; /* 8086/80188 -- tidak bisa clear bit 12-15 */
    }

    /* --- Tahap 2: 80286 vs 80386+ (BARU, lihat catatan kepala file) --- */
    if (!cpu_supports_386_instructions()) {
        return 286;
    }

    return GEN_NEEDS_EXTENDED_PROBE; /* >=80386, lanjut ke modul terpisah */
}

/* -------------------------------------------------------------------
 * read_bios_tick() / run_calib_block() / cycles_per_iter_for_generation()
 * / measure_loop_iterations() / measure_mhz_loop()
 *
 * Metode fallback untuk CPU tanpa TSC (8086-486): jalankan blok loop
 * kalibrasi (DEC CX / JNZ, 65535 iterasi) berulang-ulang selama
 * jendela waktu tertentu, hitung total iterasi, lalu konversi ke MHz
 * pakai tabel referensi siklus-per-iterasi. SEMUA instruksi di sini
 * 16-bit, aman di target -0.
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
 * berdasarkan dokumentasi timing instruksi Intel klasik. Ini
 * PENDEKATAN, bukan angka pasti -- SILAKAN DIKALIBRASI ULANG dgn
 * membandingkan hasil di 86Box terhadap clock speed yg di-set manual
 * di konfigurasi mesin virtualnya. */
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

    out->generation = flags_probe_basic();
    if (out->generation == GEN_NEEDS_EXTENDED_PROBE) {
        /* CPU dipastikan >=80386 -- lanjut ke modul terpisah yang
         * boleh pakai instruksi 386+/CPUID. */
        out->generation = flags_probe_extended();
    }

    out->has_cpuid = (out->generation >= 586);

    if (out->has_cpuid) {
        read_cpuid_strings(out);
    } else {
        const char *label;
        switch (out->generation) {
            case 86:  label = "Intel 8086/8088 atau kompatibel"; break;
            case 286: label = "Intel 80286 atau kompatibel";     break;
            case 386: label = "Intel 80386 atau kompatibel";     break;
            case 486: label = "Intel 80486 atau kompatibel";     break;
            default:  label = "Generasi CPU tidak dikenali";     break;
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
            sprintf(out->clock.text, "~%lu MHz (estimasi)", mhz);
        }
        out->clock.status = STATUS_OK;
    } else {
        strcpy(out->clock.text, "N/A (pengukuran gagal)");
        out->clock.status = STATUS_FAIL;
    }
}
