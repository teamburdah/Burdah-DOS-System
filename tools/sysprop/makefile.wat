# ===================================================================
# makefile.wat
# Build script untuk Open Watcom wmake.
#
# PENTING (temuan fase 3.5, lihat DECISIONS.md poin 9a): detect_cpu_x86.c
# WAJIB dikompilasi target CPU tinggi (-5, Pentium) karena berisi
# pushfd/popfd/CPUID/RDTSC yang DITOLAK assembler kalau target -0.
# Semua file LAIN tetap target -0 (8086 murni) supaya exe akhir tetap
# jalan di 8086 asli -- lihat detect_cpu_x86.h utk penjelasan kenapa
# ini aman (percabangan runtime memastikan kode itu cuma dieksekusi
# di CPU yang memang mendukungnya).
#
# Cara pakai:
#   cd src
#   wmake -f ..\makefile.wat
# ===================================================================

CC     = wcc
LD     = wlink
CFLAGS_BASE = -ms -zq -oh -d0
LFLAGS = system dos

OBJS = main.obj detect_os.obj detect_cpu.obj detect_cpu_x86.obj &
       detect_mem.obj detect_disk.obj detect_board.obj detect_video.obj &
       render.obj

sysprop.exe : $(OBJS)
	$(LD) $(LFLAGS) name sysprop file { $(OBJS) }

# detect_cpu_x86.obj: target -5 (Pentium) -- WAJIB, lihat catatan di atas
detect_cpu_x86.obj : detect_cpu_x86.c detect_cpu_x86.h sysinfo.h
	$(CC) -5 $(CFLAGS_BASE) detect_cpu_x86.c -fo=detect_cpu_x86.obj

# Semua file lain: target -0 (8086 murni)
.c.obj : .AUTODEPEND
	$(CC) -0 $(CFLAGS_BASE) $[*.c -fo=$^@

clean : .SYMBOLIC
	del *.obj
	del sysprop.exe
