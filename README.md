# Burdah-DOS System 0.5

**Burdah-DOS** is a remaster of the [FreeDOS](https://www.freedos.org/)
kernel and [FreeCOM](https://github.com/FDOS/freecom) shell — not merely a
rename/text replacement, but a genuine modification at the kernel, boot sector,
and command-line interpreter levels, while maintaining full compatibility with
legacy DOS software.

Primary hardware target: **Intel 80286** (tested using the Dell System 200
profile in [PCem](https://pcem-emulator.co.uk/)), with QEMU used for rapid
iteration during development.

> A complete record of the modification process — including discovered bugs
> and their fixes — is available in
> [`bootleg_info_en.md`](./bootleg_info_en.md). English
> [`bootleg_info_id.md`](./bootleg_info_id.md). Bahasa Indonesia

---

## Features Added/Modified from Original FreeDOS

- **Own OS identity**, not merely cosmetic:
  - Kernel name: `BURDAH.SYS` (instead of `KERNEL.SYS`), including its own
    boot sector loader.
  - **OEM ID** (the byte read through `INT 21h AH=30h`, used by tools such as
    `dosfetch` to identify "which DOS this is"): `0xBB`, instead of FreeDOS's
    `0xFD`.
  - OEM ID can be overridden at boot through `CONFIG.SYS`
    (`OEMID=0xFD`) for backward compatibility with older software that is
    sensitive to DOS identity — without recompiling.
  - Built-in support for the Indonesian locale (`COUNTRY=62,437`): DD/MM/YYYY
    date format, "Rp" currency, and "Ya/Tidak" confirmation.
- **New internal commands** in `COMMAND.COM` (FreeCOM):
  - `COLOR bf` — changes the console background/text colours, in the style of
    `CMD.EXE`.
  - `HELP` — a reference table of all internal commands, displayed as a
    scrollable TUI (up/down arrows, ESC to exit) because the command list does
    not fit on a single 80×25 screen.
- The kernel is deliberately assembled for the **8086** baseline (`XCPU=86`)
  to maximise compatibility from vintage PCs/XTs through modern emulators/PCs,
  rather than being optimised for a single CPU generation.

---

## Project Structure

```text
├── kernel/          # DOS kernel (BURDAH.SYS)
├── boot/            # Boot sector & bootloader
├── sys/             # SYS.COM — boot sector installer/writer
├── country/         # Localisation data (submodule, see below)
├── share/           # Shared kernel data (submodule, see below)
├── freecom/         # Shell / COMMAND.COM (separate repository, FreeCOM)
├── hdr/, drivers/, lib/, utils/, setver/   # Other kernel components
├── mkfiles/         # Build definitions per compiler
└── BURDAH-DOS-MODIFICATION-LOG.md   # Complete remastering process log
```

> **Important**: `country/` and `share/` are **git submodules**
> (`FDOS/country` and `FDOS/share`). If downloaded as a regular ZIP archive
> (rather than using `git clone`), both folders will be **empty** and the build
> will fail. See the Build section below.

---

## Build

### Prerequisites

- [Open Watcom 2](https://github.com/open-watcom/open-watcom-v2)
- [NASM](https://www.nasm.us/)
- [UPX](https://upx.github.io/) (for kernel compression)
- [mtools](https://www.gnu.org/software/mtools/) (for creating floppy images)

### Obtain the complete source (including submodules)

```bash
git clone --recurse-submodules https://github.com/<org>/burdah-dos.git
# or, if you have already cloned without them:
git submodule update --init --recursive
```

### Compile the kernel

```bash
export WATCOM=/path/to/open-watcom
export PATH=$PATH:$WATCOM/binl64
make all COMPILER=owlinux
# output: bin/kernel.sys (BURDAH.SYS), bin/sys.com, bin/country.sys, etc.
```

### Compile the shell (`COMMAND.COM`)

```bash
cd freecom
./build.sh watcom
# output: command.com
```

### Create a 1.44MB floppy image

```bash
mformat -i burdah.img -f 1440 -v BURDAH ::
mcopy -i burdah.img -o BURDAH.SYS COMMAND.COM COUNTRY.SYS CONFIG.SYS AUTOEXEC.BAT SYS.COM ::
mattrib -i burdah.img +s +h ::BURDAH.SYS ::COMMAND.COM
```

Boot `burdah.img` in QEMU (`qemu-system-i386 -fda burdah.img -boot a`) or
PCem for testing with the 286 hardware profile.

---

## Status & Pending Work

Several items have been proposed but have not yet been fully implemented —
they are documented transparently in the
[modification log](./BURDAH-DOS-MODIFICATION-LOG.md#5-pending-work--not-yet-executed),
including: standardising the `0.5` version banner across all kernel files, and
translating the documentation (the original FreeDOS `README` and `docs/`),
which has not yet been touched.

---

## Licence

Burdah-DOS is a derivative work of the
[FreeDOS kernel](https://github.com/FDOS/kernel) and
[FreeCOM](https://github.com/FDOS/freecom), and therefore **follows the
licences of the original software**:

**GNU General Public License v2.0 (GPL-2.0)** — see the
[`COPYING`](./COPYING) file.

## Credits

- [The FreeDOS Project](https://www.freedos.org/) — kernel, `COMMAND.COM`
  (FreeCOM), and all the core utilities that form the foundation of this
  project.
- The original contributors to the DOS-C kernel and FreeCOM whose work has
  been modified in this project.
