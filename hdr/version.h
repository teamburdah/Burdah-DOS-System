/****************************************************************/
/* THIS FILE IS A PART OF BURDAH-DOS 0.5                        */
/* Final Release: 17 August 2026                                */
/****************************************************************/
/*                                                              */
/*                          version.h                           */
/*                                                              */
/*                  Common version information                  */
/*                                                              */
/*                      Copyright (c) 1997                      */
/*                      Pasquale J. Villani                     */
/*                      All Rights Reserved                     */
/*                                                              */
/* This file is part of DOS-C.                                  */
/*                                                              */
/* DOS-C is free software; you can redistribute it and/or       */
/* modify it under the terms of the GNU General Public License  */
/* as published by the Free Software Foundation; either version */
/* 2, or (at your option) any later version.                    */
/*                                                              */
/* DOS-C is distributed in the hope that it will be useful, but */
/* WITHOUT ANY WARRANTY; without even the implied warranty of   */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See    */
/* the GNU General Public License for more details.             */
/*                                                              */
/* You should have received a copy of the GNU General Public    */
/* License along with DOS-C; see the file COPYING.  If not,     */
/* write to the Free Software Foundation, 675 Mass Ave,         */
/* Cambridge, MA 02139, USA.                                    */
/****************************************************************/

/* The version the kernel reports as compatible with */
#ifdef WITHFAT32
#define MAJOR_RELEASE   7
#define MINOR_RELEASE   10
#else
#define MAJOR_RELEASE   6
#define MINOR_RELEASE   22
#endif

/* The actual kernel revision, 2000+REVISION_SEQ = 2.REVISION_SEQ */
#define REVISION_SEQ    46      /* returned in BL by int 21 function 30 */
#define OEM_ID          0xbb    /* Burdah DOS System, returned in BH by int 21 30 */
                                /* (previously 0xfd, FreeDOS's OEM ID)            */
                                /* runtime-overridable via OEMID= in CONFIG.SYS   */

/* Used for version information displayed to user at boot (& stored in os_release string) */
#ifndef KERNEL_VERSION
#define KERNEL_VERSION "0.5 "
#endif

/* actual version string */
#define _KVS_STR(x)  #x
#define _KVS_XSTR(x) _KVS_STR(x)
#define KVS(v, r, o) "Burdah DOS System version " v "(build " r ", OEM:" o ")\n"
#define xKVS(v, r, o) KVS(v, _KVS_XSTR(r), _KVS_XSTR(o))
#define KERNEL_VERSION_STRING xKVS(KERNEL_VERSION, REVISION_SEQ, OEM_ID)

