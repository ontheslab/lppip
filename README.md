# LPPIP - LS-DOS File Copy Utility

A native file copy tool for TRS-80 computers running LS-DOS 6.x, LDOS, or TRSDOS.
Extends the built-in COPY command with SD card support, wildcard copies, and CRC-16 verify.

---

## Features

- Copy files between any combination of LS-DOS drives (0-7)
- Copy to and from a FreHD SD card using the `SD:` prefix
- Wildcard copies from SD card on all models (Model I, III, and 4)
- Wildcard copies from LS-DOS drives on Model 4
- CRC-16 verify pass after copy (`/V`) with optional CRC display (`/C`)
- Handles existing and read-only destination files with prompts or silent overwrite (`/E`, `/W`)
- Detects FreHD and TRS-80 model at startup - no configuration needed

---

## Requirements

- TRS-80 Model I, III, or 4 running LS-DOS 6.x, LDOS, or TRSDOS
- For LS-DOS wildcard copies from a local drive: Model 4 required
- For SD card operations: FreHD hard disk emulator with SD card inserted

---

## Quick Start

Copy `LPPIP.CMD` to any LS-DOS drive and run it:

```
LPPIP
LPPIP V0.1.69 - LS-DOS FILE COPY UTILITY [FreHD Detected]
USAGE: LPPIP SRC DEST [/V] [/C] [/E] [/W]
```

`[FreHD Detected]` confirms the SD card interface is available.

For built-in help:

```
LPPIP /H
```

---

## Usage

```
LPPIP SRC DEST [/V] [/C] [/E] [/W]
```

### File formats

| Format | Meaning |
|--------|---------|
| `NAME/EXT:d` | LS-DOS file on drive d (0-7) |
| `:d` | Bare drive - uses the source filename |
| `SD:FILE.EXT` | File at the SD card root |
| `SD:DIR/` | SD subdirectory - uses the source filename |
| `SD:DIR/FILE.EXT` | Specific file in an SD subdirectory |

### Examples

```
LPPIP SD:FILE.ASC :4              SD card to floppy drive 4
LPPIP FILE/ASC:4 :0              drive 4 to drive 0
LPPIP FILE/ASC:4 SD:backup/      LS-DOS file to SD backup/ folder
LPPIP SD:*.ASC :4                all .ASC files from SD root to drive 4
LPPIP */ASC:4 :3                 all .ASC from drive 4 to drive 3 (Model 4)
LPPIP SD:*.* :4 /V               all SD files to drive 4, verify each
LPPIP FILE/ASC:4 :3 /C           copy with verify and show CRC value
```

### Options

| Option | Effect |
|--------|--------|
| `/V` | Verify copy using CRC-16 |
| `/C` | Show CRC value in hex after verify (implies `/V`) |
| `/E` | Overwrite existing read-write files without asking |
| `/W` | Overwrite read-only files (implies `/E` for read-write) |
| `/H` | Show help screen |

Options can be combined: `/VC`, `/WV`, etc.

---

## Building from Source

Requires [zmac](http://48k.ca/zmac.html) assembler.

```
zmac --z80 -o zout\lppip.cmd -o zout\lppip.lst lppip_ldos.z80
```

The main source file is `lppip_ldos.z80`. It includes all the `.inc` modules.
Build output goes to `zout\` under the working directory.

The pinned build path used in this project:

```
C:\z88dk\zmac\zmac.exe --z80 -o zout\lppip.cmd -o zout\lppip.lst lppip_ldos.z80
```

---

## Source Layout

| File | Purpose |
|------|---------|
| `lppip_ldos.z80` | Main source - EQUs, SVC table, entry point include chain |
| `lppip_entry.inc` | Entry point and top-level copy dispatch |
| `lppip_local.inc` | LS-DOS to LS-DOS single-file copy |
| `lppip_frehd.inc` | FreHD SD card protocol, read/write, SD source copy |
| `lppip_sd_paths.inc` | SD path building, SD wildcard copy loop |
| `lppip_dos_wild.inc` | LS-DOS directory scan and wildcard copy loop |
| `lppip_exists.inc` | Destination file-exists check and overwrite handling |
| `lppip_crc.inc` | CRC-16 CCITT table, update, and verify pass |
| `lppip_display.inc` | Output formatting, hex display |
| `lppip_helpers.inc` | Token parsing, option switches, string utilities |
| `lppip_errors.inc` | Error handlers and abort paths |
| `lppip_data.inc` | Buffers, DCBs, strings, variables |
| `trsident.z80` | TRS-80 model identification (WHATVER) |

---

## Known Limitations

- LS-DOS wildcard source requires Model 4. Model I and III can use `SD:` wildcards.
- SD subdirectories must be created on a PC before use. LPPIP cannot create directories.

---

## Guide

The full user guide is included in every release zip as `lppip-guide.txt`.

---

## Related

[CPPIP](https://github.com/ontheslab/cppip) - CP/M port of PPIP for TRS-80 and NABU computers, also with FreHD SD card support.
