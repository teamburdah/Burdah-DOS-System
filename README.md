# 🖥️ Burdah-DOS System 0.5

> A FreeDOS kernel, remastered over a handful of vibe-coding sessions into something that boots, shows a logo, and (mostly) knows its own name.

Burdah-DOS is a bootleg fork of [FreeDOS](http://www.freedos.org/) (the DOS-C kernel, originally written by Pasquale J. Villani), rebranded, patched, and lightly gussied up with a couple of extra shell commands and a boot splash. It targets real vintage iron — a **Dell System 200 (Intel 80286)** — and is tested in both **PCem** (for hardware accuracy) and **QEMU** (for fast iteration).

Nothing here was written from scratch. It's FreeDOS wearing a new jacket, with a few new pockets sewn on.

---

## What actually changed

- **Full identity rebrand** — boot banner, bootloader messages, `SYS.COM`, and the kernel version string all say "Burdah" now, not "FreeDOS." (A few internal function names and license comments still say FreeDOS — that's cosmetic and doesn't affect anything at runtime.)
- **A real, unique OEM ID (`0xBB`)** — this was the sneaky one. Even with every visible banner rebranded, identity-detection tools like `dosfetch` kept reporting `FreeDOS 7.10`, because that info doesn't come from banner text — it comes from `INT 21h AH=30h`, a single byte (`BH`) that's the actual DOS vendor ID. Burdah was still silently using FreeDOS's own official ID (`0xFD`). Fixed, and made **runtime-configurable** via a new `OEMID=` line in `CONFIG.SYS`, so it can still pretend to be FreeDOS if some cranky old software needs that.
- **`COLOR` command** — CMD-style `COLOR bf` for `COMMAND.COM` (Portal, the FreeCOM-based shell), with correct errorlevel handling for bad arguments.
- **`HELP` command** — a scrollable full-screen TUI listing all 53 internal shell commands, with descriptions pulled automatically from each command's existing help text (no separate list to maintain by hand).
- **Boot splash** — a `LOGO.SYS`-style splash image (BMP, 320×200, 8bpp) shown at the kernel level, before `CONFIG.SYS` even starts processing, styled after the old Windows 9x boot splash. Manual skip and automatic close both work; text output is cleanly suppressed while it's on screen.
- **XCPU=86 kept on purpose** — the kernel is still assembled against the 8086 baseline rather than a newer target. That's intentional: `XCPU=86` runs everywhere from real 8086/286 hardware up to modern x86-64 real mode, while a newer target would lock the 286 target machine out entirely. There's also a built-in safety net: a kernel built for a specific CPU refuses to boot on the wrong hardware with a clear `cpu_abort` message instead of just crashing.

## Known rough edges (not hidden, just not done yet)

This is a vibe-coded project audited across multiple sessions, so here's what's still loose:

- Version headers (`BURDAH-DOS 0.5`) aren't in every kernel source file yet — cosmetic only.
- `docs/fdkernel.lsm` still says `Version: git`.
- `COMMAND.COM` (Portal) reports its own version independently (`0.87`) — a deliberate design choice for now, not a bug, but not yet a final decision.
- `README.md` and most of `docs/` (including `portal/README.md`) are still 100% original FreeDOS/FreeCOM text — this file you're reading is the first one actually rewritten.
- `HELP`'s full-screen redraw is BIOS-call-heavy (~1,400 `INT 10h` calls per redraw) — not broken, but could feel sluggish on real 286 hardware during fast scrolling.
- A few `.err` string files were hand-reconstructed placeholders because the original generator script wasn't in the source archive.
- Leftover build artifacts (`bin/kwc8632.sys`, `bin/kwc8632.map`, `portal/strings/fixstrs.exe`) still need cleaning out before a proper release.
- The manual boot-splash-skip keystroke path hasn't been explicitly tested via simulated keypresses yet (the logic is there, just not verified that way).

## Bugs found along the way (the honest list)

Kept here for transparency, because a fair few sessions went into these:

| # | Bug | Fix |
|---|---|---|
| 1 | Kernel build failed completely | Missing `git submodule` contents (`country/`, `share/`) — cloned manually |
| 2 | Build failed at kernel compression | `upx-ucl` wasn't installed |
| 3 | `fixstrs.exe` segfaulted adding `HELP` | Fixed string-table array size (`MAXSTRINGS` 256 → 512) |
| 4 | HELP screen kept scrolling upward on its own | Bottom-right character write was triggering BIOS teletype auto-scroll — switched to direct-position BIOS writes |
| 5 | OEM ID still `0xFD` (FreeDOS's own) | Changed to `0xBB`, made configurable via `CONFIG.SYS` |
| 6 | `ver.c` failed to compile | Case mismatch between `FreeCOM_VERSION` and `FREECOM_VERSION` — C is case-sensitive | 
| 7 | `globals.h` failed to compile | Version macro was rebranded into a plain string but still called as a function-style macro elsewhere — restored as a proper macro |
| 8 | `main.c` failed to compile | A broken `/ ... */` comment closer created an (illegal) nested comment |
| 9 | Config display showed `"DOS-C"` instead of `"Burdah"` | Missing recognition case for the new `0xBB` OEM ID |
| 10 | Portal build stopped dead — missing `.err` files | Original error-string generator wasn't in the archive; files reconstructed by hand |
| 11 | Boot splash silently never appeared | Splash loader's first file candidate had the same name as the kernel file itself, so it always "found" a file — just not a valid BMP — and gave up instead of trying `LOGO.SYS` next. Rewritten to try multiple candidates in order |
| 12 | **[Fatal]** Kernel froze solid right after the splash appeared | A stack-imbalance bug in the `INT 29h` handler in `console.asm` — a conditional jump skipped a `push` block that its matching `pop`s always ran regardless, corrupting the stack on the very first suppressed character write. Traced by poking debug markers straight into VRAM and reading them back via QEMU screendumps, since normal text output was exactly what was broken |

One thing that looked like a *second* bug during HELP debugging — a partially-drawn table border — turned out to be a testing artifact (screenshots taken faster than DOS's keyboard buffer and BIOS redraw could keep up, not an actual rendering bug). Recorded here too, since chasing it took a few turns of its own.

## Building it yourself

```bash
# 1. Kernel
export WATCOM=/path/to/open-watcom
export PATH=$PATH:$WATCOM/binl64
make all COMPILER=owlinux        # produces bin/kernel.sys, bin/kwc8632.sys, etc.
mv bin/kernel.sys bin/BURDAH.SYS # matches the hardcoded name in sys/fdkrncfg.c

# 2. Portal (COMMAND.COM)
cd portal
bash build.sh watcom
cp command.com ../bin/COMMAND.COM
cd ..

# 3. Floppy image (1.44MB)
dd if=/dev/zero of=burdahdos.img bs=1024 count=1440
mformat -i burdahdos.img -f 1440 -v BURDAH ::
dd if=boot/fat12com.bin of=burdahdos.img bs=1 count=3 conv=notrunc
dd if=boot/fat12com.bin of=burdahdos.img bs=1 skip=62 seek=62 count=450 conv=notrunc
mcopy -i burdahdos.img -o bin/BURDAH.SYS bin/COMMAND.COM bin/country.sys \
  bin/config.sys bin/bbauto.bat bin/sys.com bin/LOGO.SYS ::
mattrib -i burdahdos.img +s +h ::BURDAH.SYS ::COMMAND.COM
```

**Toolchain:** Open Watcom 2 (Linux build), NASM, UPX, `mtools`, QEMU for quick boots, PCem for the real hardware-accuracy pass.

---

### A little aside

*Is this AI slop or an actual gem? I honestly don't know how to judge it anymore.*

---

## Original FreeDOS credits

This project is built directly on top of the **FreeDOS Kernel (DOS-C)**, originally written by Pasquale J. Villani.

- Upstream project: <http://www.freedos.org/>
- Kernel source: <http://freedos.sourceforge.net/>
- Kernel issue tracker: <https://github.com/FDOS/kernel/issues>

> DOS-C is (c) Copyright 1995, 1996 by Pasquale J. Villani. All Rights Reserved.
> Portions of the FreeDOS kernel are copyright others, 199?–2024.

Burdah-DOS is an unofficial, hobbyist remaster — not affiliated with or endorsed by the FreeDOS Project.
