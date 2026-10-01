/* ===================================================================
 * detect_cpu_x86.c
 * Implementasi fungsi 386+/CPUID/RDTSC -- lihat detect_cpu_x86.h utk
 * penjelasan KENAPA file ini terpisah dari detect_cpu.c dan KAPAN
 * boleh dipanggil.
 *
 * FILE INI WAJIB DIKOMPILASI DENGAN TARGET CPU TINGGI (dipakai: -5,
 * target Pentium) SUPAYA ASSEMBLER MENGIZINKAN pushfd/popfd/EAX/
 * CPUID/RDTSC -- BEDA dari semua file lain di proyek ini yang pakai
 * -0. Lihat makefile.wat dan DECISIONS.md poin 9 untuk detail
 * temuan ini (ditemukan lewat pengujian compile nyata, bukan diduga).
 * =================================================================== */

#include "detect_cpu_x86.h"
#include <string.h>
#include <stdio.h>

#define US_PER_TICK 54925UL /* mikrodetik per PIT tick, lihat detect_cpu.c */

/* Duplikat kecil dari detect_cpu.c -- sengaja, supaya file ini tetap
 * mandiri (tak perlu header internal tambahan cuma utk 1 fungsi kecil
 * yang aman dikompilasi ulang di kedua target CPU). */
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

/* Probe minimal: HANYA pushfd/popfd, tanpa logika apapun. Dipanggil
 * exclusive lewat pembungkus setjmp/longjmp + handler INT 6 di
 * detect_cpu.c -- lihat detect_cpu_x86.h utk kontrak lengkapnya.
 *
 * KENAPA INI DIPERLUKAN (temuan lapangan): tes lama berbasis kuirk
 * bit IOPL/NT di FLAGS (coba SET bit 12-15 via POPF 16-bit, cek
 * apakah "nempel") ternyata TIDAK BISA DIANDALKAN di banyak
 * lingkungan virtualisasi (dikonfirmasi user: 86Box lintas beberapa
 * profil mesin, dan QEMU, KEDUANYA salah menyimpulkan CPU sbg 80286
 * padahal CPU sebenarnya jauh lebih baru/cepat). Dugaan kuat: hypervisor
 * memvirtualisasi/membatasi manipulasi IOPL di real mode (IOPL adalah
 * flag terkait privilege I/O, target wajar untuk dimediasi hypervisor
 * demi keamanan) -- BEDA dgn instruksi CPU sungguhan spt pushfd, yang
 * WAJIB diemulasikan benar oleh emulator/hypervisor manapun supaya
 * software lain (yg jauh lebih umum drpd utility deteksi CPU jadul)
 * tetap berjalan. Makanya tes berbasis EKSEKUSI INSTRUKSI (coba
 * jalankan pushfd, tangkap fault kalau gagal) jauh lebih dapat
 * diandalkan drpd tes berbasis KUIRK NILAI FLAG semata. */
void probe_pushfd_raw(void)
{
    _asm {
        pushfd
        popfd
    }
}


int flags_probe_extended(void)
{
    unsigned long orig32 = 0;
    unsigned long f1 = 0, f2 = 0;
    int gen;

    /* Simpan EFLAGS 32-bit PENUH sebelum diutak-atik, dipakai utk
     * restore akurat di akhir fungsi (menutup celah restore-parsial
     * yang ada di versi fase 2). */
    _asm {
        pushfd
        pop orig32
    }

    /* --- Tahap 3: 80386 vs 80486+ ---
     * Uji AC flag (bit 18 EFLAGS / Alignment Check).
     * 80386 tidak bisa men-set bit ini; 80486 ke atas bisa. */
    _asm {
        pushfd
        pop eax
        mov f1, eax
        or eax, 00040000h
        push eax
        popfd
        pushfd
        pop eax
        mov f2, eax
    }

    if ((f1 & 0x00040000UL) == (f2 & 0x00040000UL)) {
        gen = 386;
    } else {
        /* --- Tahap 4: 80486 vs Pentium+ (dukungan CPUID) ---
         * Uji ID flag (bit 21 EFLAGS). Kalau bisa di-toggle, CPU
         * mendukung instruksi CPUID. */
        _asm {
            pushfd
            pop eax
            mov f1, eax
            xor eax, 00200000h
            push eax
            popfd
            pushfd
            pop eax
            mov f2, eax
        }
        gen = ((f1 & 0x00200000UL) == (f2 & 0x00200000UL)) ? 486 : 586;
    }

    /* Restore EFLAGS 32-bit persis seperti sebelum tahap 3 dimulai. */
    _asm {
        mov eax, orig32
        push eax
        popfd
    }

    return gen;
}

/* ===================================================================
 * Tabel referensi Family/Model CPUID -> nama umum.
 *
 * HANYA dipakai kalau brand string (leaf 0x80000002-4) TIDAK tersedia
 * -- umumnya CPU generasi Pentium klasik s.d. awal Pentium II/Celeron
 * (brand string baru jadi standar luas sejak akhir era Pentium III).
 *
 * Tabel ini SENGAJA TIDAK LENGKAP (mustahil mencakup semua clone/
 * varian/stepping yang pernah ada) -- kalau kombinasi Family/Model tak
 * dikenali, tetap ditampilkan APA ADANYA ("Vendor Family X Model Y")
 * dan diberi status NA (bukan OK), supaya TIDAK PERNAH mengarang nama
 * yang tidak yakin benar. Ini konsisten dgn prinsip proyek: field yang
 * tak pasti selalu jujur ditandai, bukan ditebak.
 * =================================================================== */
typedef struct {
    unsigned char is_amd; /* 0 = Intel, 1 = AMD */
    unsigned char family;
    unsigned char model;
    const char   *name;
} cpu_model_entry_t;

static const cpu_model_entry_t cpu_model_table[] = {
    /* --- Intel Family 5 (Pentium klasik, sebelum brand string) --- */
    {0, 5, 1, "Intel Pentium (60/66 MHz, P5)"},
    {0, 5, 2, "Intel Pentium (75-200 MHz, P54C)"},
    {0, 5, 3, "Intel Pentium OverDrive"},
    {0, 5, 4, "Intel Pentium MMX (P55C)"},
    {0, 5, 7, "Intel Pentium (P54CS)"},
    {0, 5, 8, "Intel Pentium MMX (mobile)"},
    /* --- Intel Family 6 (P6 s.d. awal Pentium III/Celeron) --- */
    {0, 6, 1, "Intel Pentium Pro"},
    {0, 6, 3, "Intel Pentium II (Klamath)"},
    {0, 6, 5, "Intel Pentium II/Celeron (Deschutes)"},
    {0, 6, 6, "Intel Celeron (Mendocino)"},
    {0, 6, 7, "Intel Pentium III (Katmai)"},
    {0, 6, 8, "Intel Pentium III/Celeron (Coppermine)"},
    {0, 6, 10,"Intel Pentium III Xeon (Cascades)"},
    {0, 6, 11,"Intel Pentium III (Tualatin)"},
    /* --- AMD Family 5 (K5/K6) --- */
    {1, 5, 0, "AMD K5 (SSA5)"},
    {1, 5, 1, "AMD K5"},
    {1, 5, 2, "AMD K5"},
    {1, 5, 3, "AMD K5"},
    {1, 5, 6, "AMD K6"},
    {1, 5, 7, "AMD K6"},
    {1, 5, 8, "AMD K6-2"},
    {1, 5, 9, "AMD K6-III"},
    /* --- AMD Family 6 (K7/Athlon/Duron awal) --- */
    {1, 6, 1, "AMD Athlon (Slot A, awal)"},
    {1, 6, 2, "AMD Athlon (K75)"},
    {1, 6, 3, "AMD Duron"},
    {1, 6, 4, "AMD Athlon (Thunderbird)"},
};
#define CPU_MODEL_TABLE_COUNT (sizeof(cpu_model_table) / sizeof(cpu_model_table[0]))

static const char *lookup_cpu_model_name(int is_amd, unsigned family, unsigned model)
{
    unsigned i;
    for (i = 0; i < CPU_MODEL_TABLE_COUNT; i++) {
        if (cpu_model_table[i].is_amd == (unsigned char)(is_amd ? 1 : 0) &&
            cpu_model_table[i].family == (unsigned char)family &&
            cpu_model_table[i].model  == (unsigned char)model) {
            return cpu_model_table[i].name;
        }
    }
    return NULL; /* kombinasi tak dikenali -- caller fallback ke label generik */
}

void read_cpuid_strings(cpu_info_t *out)
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
        /* Brand string tak tersedia -- coba Family/Model dari CPUID
         * leaf 1 (EAX) utk nama yang lebih spesifik drpd sekadar
         * vendor string mentah. Lihat cpu_model_table di atas. */
        unsigned long features_eax = 0;
        unsigned base_family, base_model, ext_family, ext_model;
        unsigned family, model;
        const char *known_name;
        int is_amd = (strcmp(vendor, "AuthenticAMD") == 0);

        _asm {
            mov eax, 1
            cpuid
            mov features_eax, eax
        }

        base_family = (unsigned)((features_eax >> 8) & 0xFUL);
        base_model  = (unsigned)((features_eax >> 4) & 0xFUL);
        ext_family  = (unsigned)((features_eax >> 20) & 0xFFUL);
        ext_model   = (unsigned)((features_eax >> 16) & 0xFUL);

        /* Per spesifikasi Intel: extended family/model cuma relevan
         * kalau base_family == 0xF (utk family) atau base_family == 6
         * ATAU 0xF (utk model). Utk CPU yg kita targetkan (family
         * 5/6 klasik), ini biasanya bernilai 0 & tak berpengaruh. */
        family = (base_family == 0xF) ? (base_family + ext_family) : base_family;
        model  = (base_family == 0x6 || base_family == 0xF)
                 ? ((ext_model << 4) | base_model) : base_model;

        known_name = lookup_cpu_model_name(is_amd, family, model);

        if (known_name != NULL) {
            strncpy(out->name.text, known_name, sizeof(out->name.text) - 1);
            out->name.text[sizeof(out->name.text) - 1] = '\0';
            out->name.status = STATUS_OK;
        } else {
            /* Kombinasi Family/Model tak ada di tabel kita -- tampilkan
             * apa adanya, JANGAN mengarang nama yang tak yakin benar. */
            sprintf(out->name.text, "%.20s Family %u Model %u", vendor, family, model);
            out->name.status = STATUS_NA;
        }
    }

    (void)max_leaf;
}

int cpu_has_tsc(void)
{
    unsigned long features_edx = 0;
    _asm {
        mov eax, 1
        cpuid
        mov features_edx, edx
    }
    return (features_edx & 0x00000010UL) ? 1 : 0; /* bit 4 = TSC */
}

static unsigned long read_tsc_low(void)
{
    unsigned long lo = 0;
    _asm {
        rdtsc
        mov lo, eax
    }
    return lo;
}

unsigned long measure_mhz_rdtsc(unsigned long target_ticks)
{
    unsigned long start_tick, cur_tick;
    unsigned long tsc_start, tsc_end, tsc_delta;
    unsigned long elapsed_us;

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
    tsc_delta = tsc_end - tsc_start;

    elapsed_us = target_ticks * US_PER_TICK;
    if (elapsed_us == 0) return 0;

    return tsc_delta / elapsed_us;
}
