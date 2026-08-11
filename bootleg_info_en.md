# Modification Log: FreeDOS → Burdah-DOS System 0.5 (English)

This document records the entire process of remastering the FreeDOS kernel into
**Burdah-DOS System 0.5**, including the workflow, technical decisions, and
**all bugs discovered during the process** — recorded as-is, including issues
that initially appeared to be major bugs but turned out not to be.

Target hardware: **Dell System 200 (Intel 80286)**, tested using
**PCem** (hardware accuracy) and **QEMU** (fast iteration).

---

## 1. Toolchain Summary

| Component | Tool |
|---|---|
| Kernel compiler | Open Watcom 2 (Linux build, `owlinux`) |
| Assembler | NASM |
| Kernel compression | UPX |
| Shell / `COMMAND.COM` | FreeCOM (`github.com/FDOS/freecom`), Open Watcom |
| Floppy image creation | `mtools` (`mformat`, `mcopy`, `mattrib`) |
| Boot testing | QEMU (`qemu-system-i386`), PCem (final validation by the user) |

---

## 2. Chronological Workflow

### 2.1 Initial audit & first build attempt

Burdah-DOS 0.5 source was uploaded as a zip archive. The first audit checked
whether the string `"FreeDOS"` had already been replaced with `"Burdah"` throughout
the source.

**Finding**: identity string rebranding (boot banner, bootloader messages,
`SYS.COM`, kernel version) was **already complete and consistent**. The remaining
60 files that still mentioned "FreeDOS" were all in license comments, internal
function names (`FreeDOSmain()`), or documentation — none of which affected
runtime identity.

The first compile attempt **failed completely**. This was the first bug found.

### 2.2 `COLOR` command (after the first successful build)

After the first floppy image successfully booted, the first request was to add
the internal `COLOR bf` command (in the style of Windows CMD) to
`COMMAND.COM` (FreeCOM).

Steps:
1. `cmd/color.c` was created — using BIOS `INT 10h AH=06h` (scroll/clear window)
   to repaint the screen with the new attribute, following the pattern already
   used by `cmd/cls.c`.
2. It was registered in `shell/cmdtable.c`, `include/command.h`, `config.h`
   (`INCLUDE_CMD_COLOR`), and `cmd/makefile.mak`.
3. Help text (`HELP COLOR`) was added to `strings/DEFAULT.lng` —
   FreeCOM's string system already had multi-language infrastructure,
   so the help text automatically followed the existing localisation system.
4. It was tested by booting QEMU: the BIOS attribute change was verified
   (blue/yellow as expected with `COLOR 1E`), and `errorlevel` was verified
   for both successful and failed cases (`COLOR zz` → error, errorlevel 1).

### 2.3 Architecture discussion: `XCPU=86` vs modern PC compatibility

A reflection session examined why the kernel is assembled with `XCPU=86`
(8086 baseline) instead of targeting a newer CPU. Conclusion: `XCPU=86` is a
fully compatible superset (runs from 8086 through modern x86-64 CPUs as well,
because x86 real mode remains backward-compatible), whereas `XCPU=386`
**closes the door** to the 286 hardware that is the main target of this project.
A built-in safety mechanism was also identified: a kernel compiled for a specific
CPU (`XCPU != 86`) automatically contains CPU-checking code at boot
(`cpu_abort`) that refuses to run and displays a clear error message rather
than crashing.

### 2.4 Audit of "0.5" version consistency

A comprehensive audit of version strings throughout the source:

- The `BURDAH-DOS 0.5` banner is **consistent** in the 7 core files that already contain it.
- **Not yet consistent**: most other kernel files (`task.c`, `dosfns.c`,
  `fatfs.c`, etc.) do not have a Burdah header at all (not a version mismatch,
  but simply not yet touched).
- `hdr/version.h` → the `KERNEL_VERSION` macro is still the placeholder
  `"- GIT "`, not `"0.5"` — this is what is actually printed on the boot screen
  (`Burdah System - GIT (build ...)`).
- `docs/fdkernel.lsm` (package metadata) still says `Version: git`.
- `COMMAND.COM` (FreeCOM) reports its own version, `0.87` — a reasonable design
  (separate component, independent versioning), not a bug, but it requires a
  conscious decision on whether it should be standardised.

> **Status**: fixes for the points above (`KERNEL_VERSION` → `"0.5"`, Burdah
> headers in all files, update to `fdkernel.lsm`) **were proposed but have not
> yet been executed** — the discussion shifted to OEM ID and the HELP command
> before a decision was made. This is **pending work**, see §5.

### 2.5 Machine-readable identity: OEM ID problem

The user ran `dosfetch` (a neofetch-style tool for DOS,
`github.com/leahneukirchen/dosfetch`) on the Burdah boot image, with the result:

```text
OS: FreeDOS 7.10
```

Although all banner text already said "Burdah", `dosfetch` still detected
FreeDOS. Investigation found the root cause: the DOS identity used by
third-party software is **not** read from the banner text, but from
**INT 21h AH=30h** ("Get DOS Version"), register `BH` = *OEM number* —
a 1-byte industry convention identifying the DOS vendor. The Burdah kernel
was still using `OEM_ID = 0xFD`, which is the **official ID used by FreeDOS
itself**.

**Fix**:
1. `hdr/version.h`: `OEM_ID` was changed from `0xFD` → **`0xBB`** (a unique
   Burdah identity, checked against known vendor IDs: `0x00` IBM, `0xFF`
   generic Microsoft, `0xFD` FreeDOS, `0x05` Zenith, `0x23` Olivetti).
2. `kernel/kernel.asm`: `Version_OemID` was synchronised to `0xBB` (the value
   is manually duplicated in two places according to the project's existing
   convention — there is an explicit comment saying "must be kept in sync with VERSION.H").
3. The OEM ID was made **runtime-configurable**, rather than being only a
   compile-time constant — a new `oem_id` field was added to the LoL struct
   (`hdr/lol.h`), synchronised with the data layout in `kernel.asm`, and
   `kernel/inthndlr.c` was changed to read this variable instead of the fixed macro.
4. A new `OEMID=` directive was added to `CONFIG.SYS` (`kernel/config.c`,
   following the existing `VERSION=` pattern) — allowing the system to
   "pretend" to be FreeDOS (`OEMID=0xFD`) or use another ID at boot, without
   recompilation, in case older software has compatibility issues.
5. This was tested using a small `TESTOEM.EXE` program that directly calls
   `INT 21h AH=30h` and prints `BH` — confirming that it changed from `0xFD`
   to `0xBB` by default and could be reset to `0xFD` through `CONFIG.SYS`.

After the user patched their own `dosfetch` to recognise `0xBB`, the result
changed to `Burdah DOS System 7.10` — confirming that the entire fix chain worked.

### 2.6 `HELP` command — scrollable TUI

With 53 internal commands registered, the command list could not be displayed
all at once on an 80×25 screen (DOS text mode has no scrollback). The user
prepared a UI mock-up (cyan title bar, No/Command/Purpose table, navigation
instructions, no scrollbar to save resources) and guided the implementation.

Final design:
- Fixed 25-line layout: title (1), instructions (2–3), spacer (4),
  top table border (5), header (6), header border (7), scrollable data
  viewport (8–24, 17 lines), closing border (25, shown only when the scroll
  reaches the last command).
- Navigation: UP/DOWN arrows to scroll, ESC or `X`/`x` to exit
  (read via `cgetchar()` + `include/keys.h`, using infrastructure already
  present in FreeCOM).
- The **Purpose** column is automatically taken from the first line of each
  command's `TEXT_CMDHELP_*` text (via `getString()`) — no separate description
  table requiring manual maintenance; new commands automatically appear in
  HELP as soon as they are added.
- Rendering uses pure BIOS `INT 10h` (without direct video memory access),
  following the convention of the existing FreeCOM codebase.

---

## 3. Bugs Found (Transparent Record)

### 3.1 Real bugs & already fixed

| # | Bug | Cause | Fix |
|---|---|---|---|
| 1 | Kernel build failed completely | `git submodule` `country/` and `share/` were empty in the zip archive (not produced by `git clone --recursive`) | Manual clone from `github.com/FDOS/country` and `github.com/FDOS/share` |
| 2 | Build failed during kernel compression | `upx`/`upx-ucl` binary was not installed in the environment | `apt-get install upx-ucl` |
| 3 | `fixstrs.exe` (FreeCOM string-resource build tool) **segfaulted** when adding the `HELP` command | `#define MAXSTRINGS 256` in `strings/fixstrs.c` — fixed-size array overflow when the total string resources reached 257 | Increased `MAXSTRINGS` to 512 |
| 4 | HELP screen kept scrolling upward even though its contents were "static" | Writing a character to the bottom-right screen cell (row 25, column 80) through BIOS **teletype** (`INT 10h AH=0Eh`) triggers automatic scrolling of the entire screen — this happened to be exactly where the HELP closing border was located | All character writes in `help.c` were changed from teletype to `INT 10h AH=09h` (write directly at the position, never triggering scroll) |
| 5 | Kernel OEM ID was still `0xFD` (the official FreeDOS ID) | Direct inheritance from the original FreeDOS source; it was not covered by the first string-rebranding audit because it is not visible on screen | Changed to `0xBB` + made runtime-configurable through `OEMID=` in `CONFIG.SYS` |

### 3.2 Issues that turned out not to be bugs (recorded for process transparency)

During debugging of the HELP closing border (row 25), a symptom briefly appeared
that looked like a second, separate bug from the auto-scroll bug above: the
border appeared **only partially** or **not at all** on certain rows. The isolation
process was:

1. It was retested repeatedly by sending 40–60 `DOWN` keystrokes in rapid
   succession through the QEMU monitor (`sendkey`) — much faster than human
   typing speed — and screenshots were taken immediately afterwards.
2. The result appeared "stable" (the same across several consecutive screenshots),
   so it was initially suspected to be a real bug rather than simply a partially
   rendered frame.
3. Systematic isolation (testing each box-drawing character individually:
   `BX_BL`, `BX_BT`, `BX_BR`, then combinations of all three) — **all passed**
   when tested with reasonable delays.
4. Final conclusion: this was a **testing methodology artefact**, not a code bug.
   DOS has a limited keyboard buffer (~15 entries); sending dozens of keystrokes
   much faster than the redraw speed (each full redraw ≈1400 BIOS `INT 10h` calls)
   caused the screenshot to capture a *torn frame* — an incomplete rendering
   that did not represent the actual state.
5. After retesting with a realistic key-press pattern (one key at a time, with
   a ≈0.4–0.5 second delay to mimic a human), the closing border was proven to
   render perfectly under every condition, including immediately after the last
   command (`53 WHICH`) on row 24.

**Lesson**: this is recorded openly because several conversation turns were
spent chasing a bug that did not actually exist — so that the process is clear
rather than hidden.

---

## 4. List of Modified Files

### Kernel (`kernel/`, `hdr/`)

| File | Change |
|---|---|
| `hdr/version.h` | `OEM_ID`: `0xFD` → `0xBB` |
| `hdr/lol.h` | Added `oem_id` field to the LoL struct |
| `kernel/kernel.asm` | `Version_OemID`: `0xFD`→`0xBB`; added runtime `_oem_id` field to LoL data |
| `kernel/globals.h` | Added `extern ASM oem_id` |
| `kernel/inthndlr.c` | `INT 21h AH=30h` handler reads `oem_id` (variable) instead of `OEM_ID` (fixed macro) |
| `kernel/config.c` | New `OEMID=` directive (`CfgOemId()`, following the `sysVersion()` pattern) |

### Shell / `COMMAND.COM` (`freecom/`)

| File | Change |
|---|---|
| `cmd/color.c` | **New** — `COLOR bf` command |
| `cmd/help.c` | **New** — `HELP` command, scrollable command-table TUI |
| `include/command.h` | `cmd_color()`, `cmd_help()` prototypes |
| `shell/cmdtable.c` | Registered `COLOR`, `HELP` in the internal command table |
| `config.h` | `INCLUDE_CMD_COLOR`, `INCLUDE_CMD_HELP` |
| `cmd/makefile.mak` | Added `color.obj`, `help.obj` to the object list |
| `strings/DEFAULT.lng` | `TEXT_CMDHELP_COLOR`, `TEXT_CMDHELP_HELP` help text |
| `strings/fixstrs.c` | `MAXSTRINGS`: 256 → 512 (build bug fix) |

### Test utility (not part of the system, included on the floppy for verification)

- `TESTOEM.EXE` — small utility that calls `INT 21h AH=30h` and prints the DOS
  version and OEM ID (`BH`) for quick verification without needing `dosfetch`.

---

## 5. Pending Work / Not Yet Executed

The following items **have already been discussed and the direction agreed
upon**, but **have not yet been implemented** — recorded here so they are not
lost from view:

1. `KERNEL_VERSION` in `hdr/version.h` is still `"- GIT "`, and has not yet been
   changed to `"0.5"` — this determines what is printed in the boot banner
   (`Burdah System - GIT (...)` should become `Burdah System 0.5 (...)`).
2. The `BURDAH-DOS 0.5` banner header has not yet been added to most kernel
   files (`task.c`, `dosfns.c`, `fatfs.c`, etc.) — cosmetic consistency only;
   it does not affect functionality.
3. `docs/fdkernel.lsm` still says `Version: git`, not `0.5`.
4. Decision on the `COMMAND.COM` version (`FreeCom version 0.87`) — whether
   to leave it independent or standardise it under the Burdah versioning scheme.
5. `README.md` and other documentation in `docs/` are still entirely in the
   original FreeDOS language and have not been touched at all.
6. `HELP` redraw is relatively heavy on the BIOS side (~1400 `INT 10h` calls
   per full redraw) — not a functional problem yet, but it may feel somewhat
   slow on real 286 hardware during rapid scrolling. A candidate for
   optimisation if needed.

---

## 6. Rebuilding the System (Summary)

```bash
# 1. Kernel
export WATCOM=/path/to/open-watcom
export PATH=$PATH:$WATCOM/binl64
make all COMPILER=owlinux        # output: bin/kernel.sys, bin/kwc8632.sys, etc.

# 2. FreeCOM (COMMAND.COM)
cd freecom
./build.sh watcom                # output: command.com

# 3. 1.44MB floppy image
mformat -i burdah.img -f 1440 -v BURDAH ::
# write boot sector (boot/fat12com.bin, preserving the BPB from mformat)
mcopy -i burdah.img -o BURDAH.SYS COMMAND.COM COUNTRY.SYS CONFIG.SYS AUTOEXEC.BAT SYS.COM ::
mattrib -i burdah.img +s +h ::BURDAH.SYS ::COMMAND.COM
```

Full details for each step (including why the submodules must be cloned
manually) are provided in §2 and §3 of this document.
