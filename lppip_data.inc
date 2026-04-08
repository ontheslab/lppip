; ----------------------------------------------------------------------
; File control blocks and filespec string buffers
; ----------------------------------------------------------------------

SRC_NAME    DEFS    SPEC_SIZE       ; src filespec string (CR-terminated)
DST_NAME    DEFS    SPEC_SIZE       ; dest filespec string (CR-terminated)
SRC_TMPL    DEFS    SPEC_SIZE       ; saved wildcard source template
DST_TMPL    DEFS    SPEC_SIZE       ; saved wildcard dest template

SRC_DCB     DEFS    DCB_SIZE        ; src file control block
IOBUF       DEFS    256             ; src I/O buffer -- must follow SRC_DCB immediately so
                                    ; DCB_BUF in SRC_DCB points here by layout (COPY_WRITE
                                    ; sets DST_DCB+DCB_BUF to IOBUF explicitly)

DST_DCB     DEFS    DCB_SIZE        ; dest file control block
DST_BUF     DEFS    256             ; dest buffer for @INIT (separate from IOBUF)
SD_DIRBUF   DEFS    SPEC_SIZE       ; SD wildcard directory prefix
SD_WILDBUF  DEFS    SPEC_SIZE       ; SD wildcard name pattern
SD_MATCHBUF DEFS    SPEC_SIZE       ; current matched SD basename
SD_FINFO    DEFS    32              ; FreHD READDIR entry buffer (FILINFO struct)
SD_MATCHTAB DEFS    SD_MATCH_LEN * SD_WILD_MAX  ; table of matched SD filenames
SD_NAMEBUF  DEFS    SPEC_SIZE       ; built SD path (CR-terminated)
SD_OPENBUF  DEFS    SPEC_SIZE + 2   ; [flags][path][NUL] packet for SD OPENFILE command
DOS_WILDBUF DEFS    11              ; 8.3 wildcard pattern for LS-DOS directory scan
DOS_MATCHBUF DEFS   SPEC_SIZE       ; current matched LS-DOS filename (8.3 as NAME/EXT)
DOS_HITBUF  DEFS    256             ; Model 4 HIT sector (kept for BSS layout, not used by scan)
DOS_WRAP    DEFB    0               ; @DIRRD wrap point: 0 = no wrap, scan all 256 DECs
SENTINEL    DEFS    32              ; first valid DEC seen, used to detect @DIRRD wrap-around
SENTINEL_FLAG DEFB  0               ; 0 = sentinel not yet captured, 1 = captured

; ----------------------------------------------------------------------
; Strings
; ----------------------------------------------------------------------

MSG_BANNER  DEFB    'LPPIP V0.1.69 - LS-DOS FILE COPY UTILITY',CR
MSG_SD_FOUND    DEFB    ' [FreHD Detected]',CR
MSG_SD_NOTFOUND DEFB    ' [FreHD not found]',CR
MSG_ARROW   DEFB    ' -> ',CR
MSG_OK      DEFB    ' OK',CR
MSG_USAGE   DEFB    'USAGE: LPPIP SRC DEST [/V] [/C] [/E] [/W]',CR
MSG_HELP_1  DEFB    CR
MSG_HELP_2  DEFB    'SRC/DEST FORMATS:',CR
MSG_HELP_3  DEFB    '  NAME/EXT:d   LS-DOS FILE ON DRIVE d (0-7)',CR
MSG_HELP_4  DEFB    '  :d           BARE DRIVE (USES SOURCE NAME)',CR
MSG_HELP_5  DEFB    '  SD:FILE.EXT  FREHD SD CARD FILE',CR
MSG_HELP_6  DEFB    '  SD:DIR/      SD SUBDIRECTORY',CR
MSG_HELP_7  DEFB    CR
MSG_HELP_8  DEFB    'WILDCARDS (DEST MUST BE :d OR SD:DIR/):',CR
MSG_HELP_9  DEFB    '  */EXT:d      LS-DOS WILDCARD - MODEL 4 ONLY',CR
MSG_HELP_A  DEFB    '  SD:*.EXT     SD WILDCARD - ALL MODELS',CR
MSG_HELP_B  DEFB    CR
MSG_HELP_C  DEFB    'OPTIONS:',CR
MSG_HELP_D  DEFB    '  /V  VERIFY AFTER COPY (CRC-16)',CR
MSG_HELP_E  DEFB    '  /C  DISPLAY CRC VALUE (IMPLIES /V)',CR
MSG_HELP_F  DEFB    '  /E  OVERWRITE EXISTING WITHOUT ASKING',CR
MSG_HELP_G  DEFB    '  /W  OVERWRITE READ-ONLY FILES',CR
MSG_HELP_H  DEFB    '  /H  THIS HELP',CR
MSG_BADSPEC DEFB    'BAD FILESPEC',CR
MSG_ERR_SRC DEFB    'CANNOT OPEN SOURCE:',CR
MSG_ERR_DST DEFB    'CANNOT CREATE DEST:',CR
MSG_ERR_CLOSE DEFB  'CLOSE ERROR:',CR
MSG_VERIFY  DEFB    ' - Verifying',CR
MSG_CRC     DEFB    '  CRC: ',0      ; NUL-terminated (not CR) -- printed by PSTRZ, not DSPLY
MSG_CRCFAIL DEFB    'CRC FAILED',CR
MSG_ERR_VDST DEFB   'CANNOT VERIFY DEST:',CR
MSG_ERR_VSD  DEFB   'CANNOT VERIFY SD DEST: ',CR
MSG_NO_SDFILES DEFB 'NO SD FILE(S) FOUND',CR
MSG_NO_DOSFILES DEFB 'NO FILE(S) FOUND',CR
MSG_ERR_WILDDST DEFB 'WILDCARD DEST MUST BE BARE DRIVE',CR
MSG_ERR_WILDM4 DEFB 'LOCAL WILDCARDS CURRENTLY NEED MODEL 4',CR
MSG_ERR_NOSD DEFB   'FREHD NOT FOUND',CR
MSG_ERR_SD2SD DEFB  'SD: TO SD: COPY NOT SUPPORTED',CR
MSG_ERR_SDNAME DEFB 'INVALID OR MISSING SD: NAME',CR
MSG_ERR_SDSRC DEFB  'CANNOT OPEN SD SOURCE: ',CR
MSG_ERR_SDDST DEFB  'CANNOT CREATE SD DEST: ',CR
MSG_SD_PREFIX_Z DEFB 'SD:',0
MSG_ERR_SDIO0 DEFB  ' FREHD I/O ERROR',0
MSG_ERR_SDIO1 DEFB  ' STATUS=',0
MSG_ERR_SDIO2 DEFB  ' ERR=',0
MSG_EXISTS  DEFB    ' EXISTS: OVERWRITE? (Y/N)',CR
MSG_SKIP    DEFB    ' SKIPPED',CR
MSG_SKIP_RO DEFB    ' SKIPPED (R/O)',CR
MSG_SKIP_LONG DEFB  ' SKIPPED (NAME > 8.3)',CR

; ----------------------------------------------------------------------
; Variables
; ----------------------------------------------------------------------

CMDPTR      DEFW    0               ; saved HL on entry (command line pointer)
MODELID     DEFB    0               ; WHATVER result: 1=Model I, 3=Model III, 4=Model 4
SRCPTR      DEFW    0               ; command-line pointer to start of source token
DSTPTR      DEFW    0               ; command-line pointer to start of dest token
OPT_VERIFY  DEFB    0               ; 1 = /V verify after copy
OPT_REPORT  DEFB    0               ; 1 = /C show CRC value (implies OPT_VERIFY)
OPT_EMEND   DEFB    0               ; 1 = /E overwrite existing R/W without prompting
OPT_WIPE    DEFB    0               ; 1 = /W overwrite existing R/O without prompting
BATCH_MODE  DEFB    0               ; 1 during wildcard loops, 0 for single-file copy
SRC_IS_SD   DEFB    0               ; 1 = source has SD: prefix
DST_IS_SD   DEFB    0               ; 1 = dest has SD: prefix
SD_ERRCODE  DEFB    0               ; last FreHD error byte from STATUS port
SD_STATUSB  DEFB    0               ; last FreHD status byte
SD_LASTEOF  DEFB    0               ; byte count of last partial SD record
SD_OPENLEN  DEFB    0               ; path length for SD OPENFILE packet
SRC_REMAIN  DEFW    0               ; records remaining in source file (from SRC_INIT_RECS)
DST_REMAIN  DEFW    0               ; records remaining in dest file (verify pass)
SD_PATHPTR  DEFW    0               ; pointer to current SD path string (NUL-terminated)
SD_PATPTR   DEFW    0               ; wildcard match: pointer into pattern string
SD_STRPTR   DEFW    0               ; wildcard match: pointer into subject string
SD_PATSTAR  DEFW    0               ; wildcard match: saved pattern position after '*'
SD_STRSTAR  DEFW    0               ; wildcard match: saved subject position after '*'
WILD_COUNT  DEFW    0               ; number of files copied in current wildcard run
WILD_INDEX  DEFW    0               ; current index into SD_MATCHTAB
DOS_DRVNO   DEFB    0               ; resolved LS-DOS drive number for wildcard scan
DOS_DECNUM  DEFB    0               ; DEC loop counter for ENUM_DOS_DIR (memory, not reg --
                                    ; V0.1.43: E is clobbered by DOS_DIR_MATCH via LD DE,DOS_WILDBUF)
CRCVAL      DEFW    0               ; running CRC-16 accumulator
CRCVAL2     DEFW    0               ; source CRC snapshot saved before verify pass
CRCTBL      DEFS    512             ; CRC-16 lookup table: 256 high bytes then 256 low bytes
                                    ; (page-aligned: INITCRC uses INC H to address the low page)
KEYIN_BUF   DEFS    4               ; Model I/III @KEYIN line input buffer (Y/N + CR + guard)
