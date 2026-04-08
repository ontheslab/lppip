# LPPIP - LS-DOS File Copy Utility
## V0.1.69 Beta  [Document revision 1.01]

*A native LS-DOS file copy tool for TRS-80 computers with FreHD SD card support.*

---

## Contents

1. [What is LPPIP?](#what-is-lppip)
2. [Hardware Requirements](#hardware-requirements)
3. [Quick Start](#quick-start)
4. [File Specification Formats](#file-specification-formats)
5. [Copying Single Files](#copying-single-files)
6. [Wildcards](#wildcards)
7. [SD Card with FreHD](#sd-card-with-frehd)
8. [CRC Verify](#crc-verify)
9. [Overwrite and Read-Only Handling](#overwrite-and-read-only-handling)
10. [All Examples](#all-examples)
11. [Options Reference](#options-reference)
12. [Messages](#messages)
13. [Troubleshooting](#troubleshooting)
14. [Known Limitations](#known-limitations)

---

## What is LPPIP?

LPPIP is a file copy utility for TRS-80 computers running LS-DOS (or LDOS / TRSDOS).
It extends the capabilities of the built-in COPY command with:

- Copy files between any combination of LS-DOS drives (0-7)
- Copy to and from a FreHD SD card using the `SD:` prefix
- Wildcard copies (all files matching a pattern) - Model 4 only for LS-DOS source
- SD card wildcards on all models
- Optional CRC-16 verify pass after every copy
- Automatic handling of existing and read-only destination files

LPPIP runs directly under LS-DOS. It does not require CP/M.

---

## Hardware Requirements

- TRS-80 Model I, III, or 4 running LS-DOS 6.x, LDOS, or TRSDOS
- For LS-DOS wildcard copies from a local drive: Model 4 required
- For SD card operations: FreHD hard disk emulator with an SD card inserted
- No special configuration needed - LPPIP detects hardware automatically at startup

---

## Quick Start

Run LPPIP with no arguments to confirm it is working:

```
LPPIP
LPPIP V0.1.69 - LS-DOS FILE COPY UTILITY [FreHD Detected]
USAGE: LPPIP SRC DEST [/V] [/C] [/E] [/W]
```

`[FreHD Detected]` confirms the FreHD SD card interface is available.

For full help at any time:

```
LPPIP /H
```

---

## File Specification Formats

LPPIP uses LS-DOS style filespecs throughout. The SD card uses the `SD:` prefix.

### LS-DOS files

```
NAME/EXT:d        full filespec - NAME up to 8 chars, EXT up to 3, drive d (0-7)
:d                bare drive only - LPPIP uses the source filename automatically
```

Examples:

```
FILE/ASC:4        FILE.ASC on drive 4
PROGRAM/CMD:0     PROGRAM.CMD on drive 0
:3                bare drive 3 (dest only - uses source name)
```

### SD card files (FreHD)

```
SD:FILE.EXT       file at the root of the SD card
SD:SUBDIR/        subdirectory (for dest - uses source name)
SD:SUBDIR/FILE    specific file in a subdirectory
```

- SD card filenames use a dot separator (FILE.EXT). LS-DOS filenames use a slash (FILE/EXT).
- LPPIP converts between the two formats automatically.
- SD file and directory names are not case-sensitive. All names are shown in uppercase.

---

## Copying Single Files

Basic copy from SD card to LS-DOS drive:

```
LPPIP SD:FILE.ASC :4
```

This copies FILE.ASC from the SD card root to drive 4, giving it the LS-DOS name
FILE/ASC. The `:4` bare drive form means LPPIP picks the destination name from
the source.

Copy from LS-DOS drive to another drive:

```
LPPIP FILE/ASC:4 :0
```

Copy from LS-DOS drive to SD card:

```
LPPIP FILE/ASC:4 SD:test/
```

The file is written to the `test` subdirectory on the SD card as FILE.ASC.

Copy from SD subdirectory:

```
LPPIP SD:test/FILE.ASC :4
```

The directory prefix is stripped for the destination name - the result is FILE/ASC:4,
not TEST/FILE/ASC:4.

---

## Wildcards

### SD card wildcards (all models)

Use `*` in the filename or extension to match multiple files:

```
LPPIP SD:*.ASC :4             all .ASC files from SD root to drive 4
LPPIP SD:test/*.ASC :4        all .ASC files from SD test/ subdir to drive 4
LPPIP SD:*.* :4               all files from SD root to drive 4
```

The destination for a wildcard copy must be a bare drive (`:d`) or an SD
subdirectory (`SD:DIR/`). A specific filename in the destination is not allowed.

### LS-DOS wildcards (Model 4 only)

Use `*` in the source spec to match files on a local LS-DOS drive. The wildcard
applies to the extension field:

```
LPPIP */ASC:4 :3              all .ASC files from drive 4 to drive 3
LPPIP */CMD:0 :4              all .CMD files from drive 0 to floppy drive 4
LPPIP */ASC:4 SD:backup/      all .ASC files from drive 4 to SD backup/ folder
LPPIP */*:4 :3                all files (any extension) from drive 4 to drive 3
LPPIP */*:4 SD:backup/        all files from drive 4 to SD backup/ folder
```

LS-DOS wildcard source requires Model 4. The directory scan used by LPPIP is only
available on the Model 4.

On a volume with stale or damaged directory sectors, LPPIP may print
`CANNOT OPEN SOURCE: / SKIPPED` for one or more unreadable entries before
continuing. The copy batch carries on normally after any skipped entries.

### Long filenames on SD card

The SD card can store names longer than 8 characters. LS-DOS can only store names
up to 8 characters long. If a wildcard matches an SD file whose name
part is longer than 8 characters, that file is skipped with a warning:

```
SD:TOOLONGNAME.ASC
 SKIPPED (NAME > 8.3)
```

The batch continues with the remaining files. To copy a long-named file, specify
it explicitly with a short destination name:

```
LPPIP SD:TOOLONGNAME.ASC SHORTNAM/ASC:4
```

---

## SD Card with FreHD

FreHD is a hard disk emulator for the TRS-80 by Frederic Vecoven. LPPIP uses
FreHD to read and write files on the SD card.

On startup, LPPIP checks for FreHD hardware. The banner shows the result:

```
[FreHD Detected]       SD: commands are available
[FreHD not found]      SD: commands will fail - check connections
```

SD card subdirectories must already exist before you copy to them. LPPIP cannot
create directories. Create the required folder structure on a PC using a card
reader, then bring the SD card back to the FreHD.

---

## CRC Verify

Add `/V` to any copy command to enable verify. After the copy completes,
LPPIP re-reads the destination and compares it to the source using a CRC checksum.

```
LPPIP SD:FILE.ASC :4 /V             verify after SD to LS-DOS copy
LPPIP FILE/ASC:4 :3 /V              verify after LS-DOS to LS-DOS copy
LPPIP */ASC:4 :3 /V                 verify every file in a wildcard batch
```

Add `/C` to also print the CRC value in hexadecimal:

```
LPPIP SD:FILE.ASC :4 /C             verify and show CRC value
```

`/C` implies `/V` - you do not need to specify both.

If a CRC mismatch is detected, the message `CRC FAILED` is shown and the
destination file is left in place but may be incomplete. Retry the copy.

---

## Overwrite and Read-Only Handling

When the destination file already exists, LPPIP asks before overwriting:

```
FILE/ASC:4 EXISTS: OVERWRITE? (Y/N)
```

Press `Y` to overwrite, `N` to skip. If the destination is read-only:

```
FILE/ASC:4 EXISTS: OVERWRITE? (Y/N) SKIPPED (R/O)
```

Even answering `Y` will not overwrite a read-only file unless `/W` is used.

### /E - overwrite without asking

```
LPPIP SD:FILE.ASC :4 /E
```

Silently overwrites existing read-write files without a prompt. Read-only files
are still protected unless `/W` is also given.

### /W - overwrite read-only files

```
LPPIP SD:FILE.ASC :4 /W
```

Removes the read-only attribute and overwrites. Implies `/E` for read-write files.

---

## All Examples

Single-file copies:

```
LPPIP SD:FILE.ASC :4               copy from SD root to floppy drive 4
LPPIP SD:test/FILE.ASC :4          copy from SD test/ subdir to drive 4
LPPIP FILE/ASC:4 :0                copy from floppy drive 4 to hard drive 0
LPPIP FILE/ASC:4 FILE/ASC:0 /V     copy with CRC verify
LPPIP FILE/ASC:4 FILE/ASC:0 /C     copy with verify and show CRC value
LPPIP FILE/ASC:4 SD:test/          copy LS-DOS file to SD test/ subdir
LPPIP SD:FILE.ASC :4 /E            copy, overwrite without asking
LPPIP SD:FILE.ASC :4 /W            copy, overwrite even if dest is R/O
```

Wildcard copies:

```
LPPIP SD:*.ASC :4                  all SD root .ASC files to floppy drive 4
LPPIP SD:test/*.ASC :4 /V          all SD test/ .ASC files to drive 4, verify each
LPPIP SD:*.* :3                    all files from SD root to hard drive 3
LPPIP */ASC:4 :3                   all .ASC files from floppy 4 to drive 3 (M4)
LPPIP */ASC:4 SD:backup/           all .ASC from floppy 4 to SD backup/ (M4)
LPPIP */*:4 :3                     all files from floppy 4 to drive 3 (M4)
LPPIP */*:4 SD:backup/             all files from floppy 4 to SD backup/ (M4)
```

Help:

```
LPPIP /H                           show full help screen
```

---

## Options Reference

| Option | Meaning |
|--------|---------|
| `/V` | Verify copy using CRC-16 checksum. Re-reads destination after copy and compares. |
| `/C` | Display CRC value in hex after verify. Implies `/V`. |
| `/E` | Overwrite existing read-write destination files without asking. |
| `/W` | Overwrite read-only destination files (also implies `/E` for read-write). |
| `/H` | Display help screen. |

Options are not case-sensitive: `/v`, `/V`, `/VC`, `/vc` all work.

Multiple options can be combined: `LPPIP SD:*.ASC :4 /VE`

---

## Messages

| Message | Meaning |
|---------|---------|
| `[FreHD Detected]` | FreHD hardware found at startup. SD: operations are available. |
| `[FreHD not found]` | FreHD not detected. SD: operations will fail. |
| `-> ... OK` | File copied successfully. |
| `- Verifying OK` | CRC verify passed. |
| `CRC: xxxx` | CRC-16 value in hex (with `/C`). |
| `CRC FAILED` | Source and destination checksums do not match. |
| `EXISTS: OVERWRITE? (Y/N)` | Destination already exists. Press Y or N. |
| `SKIPPED` | User pressed N at the overwrite prompt. |
| `SKIPPED (R/O)` | Destination is read-only and `/W` was not given. |
| `SKIPPED (NAME > 8.3)` | SD wildcard matched a file with a name too long for LS-DOS. |
| `CANNOT OPEN SD SOURCE:` | SD source file not found or could not be read. |
| `CANNOT CREATE SD DEST:` | SD destination could not be created. Check the subdir exists. |
| `CANNOT OPEN SOURCE: / SKIPPED` | LS-DOS source file not found or unreadable. During a wildcard batch, LPPIP skips the entry and continues. |
| `CANNOT CREATE DEST:` | LS-DOS destination could not be created. Drive may be full. |
| `FREHD NOT FOUND` | SD: command used but FreHD is not present. |
| `SD: TO SD: COPY NOT SUPPORTED` | Both source and destination used SD: prefix. |
| `WILDCARD DEST MUST BE BARE DRIVE` | Wildcard dest must be `:d` or `SD:DIR/`. |
| `LOCAL WILDCARDS CURRENTLY NEED MODEL 4` | LS-DOS wildcard source used on non-Model 4. |
| `NO FILE(S) FOUND` | Wildcard matched no files. |

---

## Troubleshooting

**Banner shows `[FreHD not found]`**

- Check that the FreHD unit is powered and connected.
- Check the SD card is inserted in the FreHD.
- LPPIP checks for FreHD hardware on startup. If the check fails or there is no
  response, FreHD is treated as not present.

**`CANNOT OPEN SD SOURCE:` for a file you can see on the card**

- Check the spelling of the filename - SD names are case-insensitive but must
  be typed without typos.
- Check whether the file is in a subdirectory. Use `SD:SUBDIR/FILE.EXT` not
  `SD:FILE.EXT` if the file is not in the SD root.

**`CANNOT CREATE SD DEST:` during a wildcard copy**

- The destination subdirectory does not exist on the SD card.
- Create the directory on a PC using a card reader first.
- The copy stops at the first failure - no further files are attempted.

**LS-DOS wildcard copies too many files, or same file copied twice**

- This was a known bug (fixed in V0.1.64) affecting floppy drives with a
  stale directory index. If you see duplicates, ensure you are running V0.1.64
  or later.

**`CANNOT OPEN SOURCE: / SKIPPED` during a wildcard batch**

- The directory on this volume contains a stale or damaged entry. LPPIP
  skips it and continues. All valid files will still be copied.
- This is most common with SD card LS-DOS volumes that were not cleanly
  formatted. It is not a problem with the files themselves.

**Wildcard finds no files (`NO FILE(S) FOUND`)**

- Check the drive number and pattern. Wildcards on LS-DOS local drives require
  Model 4.
- On a floppy, if the floppy was recently inserted, the directory may not be
  refreshed. Try accessing the drive first (e.g. `DIR :4`) before running LPPIP.

**CRC FAILED after a copy**

- Retry the copy. A single CRC failure on a hard disk copy is unusual.
- On floppy, check the disk condition. Try a different floppy if available.
- On SD card, remove and re-insert the card and retry.

---

## Known Limitations

- LS-DOS wildcard source requires Model 4. Model I and III can use SD: wildcards.
- SD: subdirectories must be created on a PC before use. LPPIP cannot create them.

---

## Change Log

**V0.1.69 Beta**
- Wildcard batch (`*/*` and `*/EXT`) now skips stale or damaged directory
  entries cleanly instead of crashing or stopping early. Valid files continue
  to copy normally after any skipped entry.
- Blank directory entries (all-spaces name) are now silently skipped in both
  LS-DOS and SD card wildcard paths.

**V0.1.65 Beta**
- Added `/H` help screen.
- Initial public beta release.

---

*LPPIP V0.1.69 Beta - Intangybles 2026*
