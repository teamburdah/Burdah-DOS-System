CFG_DEPENDENCIES = tools.m1 ../utils/mktools.c tools.m2

TOP = ..
!include "$(TOP)/config.mak"
all : $(CFG) ptchsize.exe kssf.com vspawn.com icmd 28.com 50.com

error:
	errlvl 127

icmd : load_icd.exe icmd_1.icd icmd_2.icd icmd_3.icd

#		*Implicit Rules*
.asm.com:
	$(NASM) $(NASMFLAGS) -f bin -o $*.com  $<
#.asm.obj:
#	$(NASM) $(NASMFLAGS) -f obj $<

icmd_1.icd : icmd_inc.inc icmd_1.nas
	$(NASM) $(NASMFLAGS) -o icmd_1.icd icmd_1.nas

icmd_2.icd : icmd_inc.inc icmd_2.nas
	$(NASM) $(NASMFLAGS) -o icmd_2.icd icmd_2.nas

icmd_3.icd : icmd_inc.inc icmd_3.nas
	$(NASM) $(NASMFLAGS) -o icmd_3.icd icmd_3.nas

load_icd.exe: load_icd.obj
	$(CL) $(CLO) load_icd.obj $(SUPPL_LIB_PATH)$(DIRSEP)suppl_$(SHELL_MMODEL).lib $(LIBC)
