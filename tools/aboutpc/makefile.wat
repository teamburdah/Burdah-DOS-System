# ===================================================================
# makefile.wat
# Build script untuk Open Watcom wmake.
# Target: instruksi set 8086 murni (-0), memory model Small (-ms),
# output .EXE DOS real-mode tunggal (tanpa DLL/overlay).
#
# Cara pakai:
#   cd src
#   wmake -f ..\makefile.wat
# ===================================================================

CC     = wcc
LD     = wlink
CFLAGS = -0 -ms -zq -oh -d0
LFLAGS = system dos

OBJS = main.obj detect_os.obj detect_cpu.obj detect_mem.obj detect_disk.obj &
       detect_board.obj detect_video.obj render.obj

aboutpc.exe : $(OBJS)
	$(LD) $(LFLAGS) name aboutpc file { $(OBJS) }

.c.obj : .AUTODEPEND
	$(CC) $(CFLAGS) $[*.c

clean : .SYMBOLIC
	del *.obj
	del aboutpc.exe
