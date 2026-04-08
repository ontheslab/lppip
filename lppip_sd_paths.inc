; ----------------------------------------------------------------------
; SD copy paths
; ----------------------------------------------------------------------

COPY_SD_WILD_TO_DOS
            CALL    SD_PRESENT
            JP      NZ, ERR_NOSD

            LD      HL, SRC_NAME
            LD      DE, SRC_TMPL
            CALL    COPYCRSTR
            LD      HL, DST_NAME
            LD      DE, DST_TMPL
            CALL    COPYCRSTR

            LD      HL, DST_TMPL
            LD      A, (HL)
            CP      ':'
            JP      NZ, ERR_WILDDST

            LD      HL, SRC_TMPL + 3
            LD      (SD_PATHPTR), HL
            CALL    PREP_SD_WILD
            CALL    SD_ENUM_WILD
            JP      C, ERR_SDIO_CLOSE_SD

            LD      HL, (WILD_COUNT)
            LD      A, H
            OR      L
            JR      NZ, SDW_HAVE
            LD      HL, MSG_NO_SDFILES
            CALL    DSPLY
            CALL    FEXIT

SDW_HAVE
            LD      A, 1
            LD      (BATCH_MODE), A
            XOR     A
            LD      (WILD_INDEX), A
            LD      (WILD_INDEX + 1), A

SDW_LOOP
            LD      HL, (WILD_INDEX)
            LD      DE, (WILD_COUNT)
            OR      A
            SBC     HL, DE
            JR      NC, SDW_DONE

            CALL    SD_LOAD_MATCH
            CALL    BUILD_SD_SOURCE_MATCH
            CALL    SD_NAME_FITS_83
            JR      NZ, SDW_LONGNAME    ; skip files with names > 8.3
            CALL    BUILD_DOS_DST_MATCH
            JP      NZ, ERR_WILDDST

            LD      HL, SRC_NAME
            LD      (SRCPTR), HL
            LD      HL, DST_NAME
            LD      (DSTPTR), HL
            CALL    SHOW_COPY_NAMES
            CALL    DEST_EXISTS_DOS
            JR      NZ, SDW_INC        ; skip: user said N / R/O without /W
            CALL    COPY_SD_TO_DOS

SDW_INC
            LD      HL, (WILD_INDEX)
            INC     HL
            LD      (WILD_INDEX), HL
            JR      SDW_LOOP

SDW_LONGNAME
            ; File name exceeds 8.3 -- print source name + skip message, continue.
            LD      HL, SRC_NAME
            LD      (SRCPTR), HL
            CALL    PSTR_NC
            LD      HL, MSG_SKIP_LONG
            CALL    DSPLY_NL
            JR      SDW_INC

SDW_DONE
            XOR     A
            LD      (BATCH_MODE), A
            CALL    FEXIT

COPY_SD_TO_DOS
            CALL    SD_PRESENT
            JP      NZ, ERR_NOSD

            LD      HL, SRC_NAME + 3
            LD      A, (HL)
            CP      CR
            JP      Z, ERR_SDNAME
            LD      (SD_PATHPTR), HL

            CALL    SD_OPEN_RD
            JP      NZ, ERR_OPEN_SDSRC

            LD      A, (OPT_VERIFY)
            OR      A
            CALL    NZ, CRC_RESET

            CALL    SD_READ_COUNT
            JP      NZ, ERR_SDIO_CLOSE_SD
            LD      A, B
            OR      C
            JR      Z, SD2DOS_EMPTY
            LD      A, (OPT_VERIFY)
            OR      A
            JR      Z, SD2DOS_HAVECRC0
            PUSH    BC
            CALL    CRC_UPDATE_IOBUF_BC
            POP     BC
SD2DOS_HAVECRC0
            PUSH    BC
            JR      SD2DOS_HAVE_DATA

SD2DOS_HAVE_DATA
            LD      HL, (DSTPTR)
            LD      DE, DST_DCB
            CALL    FSPEC
            JP      NZ, ERR_SPEC_CLOSE_SD

            LD      HL, IOBUF
            LD      B, 0
            LD      DE, DST_DCB
            CALL    FINIT
            JR      Z, SD2DOS_INIT_OK
            CP      42
            JP      NZ, ERR_OPEN_DST_CLOSE_SD
SD2DOS_INIT_OK
            JP      SD2DOS_WRITE

SD2DOS_EMPTY
            LD      HL, (DSTPTR)
            LD      DE, DST_DCB
            CALL    FSPEC
            JP      NZ, ERR_SPEC_CLOSE_SD

            LD      HL, IOBUF
            LD      B, 0
            LD      DE, DST_DCB
            CALL    FINIT
            JR      Z, SD2DOS_EMPTY_DONE
            CP      42
            JP      NZ, ERR_OPEN_DST_CLOSE_SD
SD2DOS_EMPTY_DONE
            LD      C, 0
            LD      A, (OPT_VERIFY)
            OR      A
            CALL    NZ, CRC_SAVE_SOURCE
            JR      SD2DOS_DONE

SD2DOS_LOOP
            CALL    SD_READ_COUNT
            JP      NZ, ERR_SDIO_CLOSE_BOTH
            LD      A, B
            OR      C
            JR      Z, SD2DOS_DONE
            LD      A, (OPT_VERIFY)
            OR      A
            JR      Z, SD2DOS_HAVECRC1
            PUSH    BC
            CALL    CRC_UPDATE_IOBUF_BC
            POP     BC
SD2DOS_HAVECRC1
            PUSH    BC

SD2DOS_WRITE
            POP     BC
            PUSH    BC
            LD      HL, IOBUF
            LD      DE, DST_DCB
            INC     B
            LD      A, C
            AND     A
            JR      Z, SD2DOS_WSTRT
SD2DOS_WLOOP
            LD      (DST_DCB + DCB_BUF), HL
            CALL    FWRITE
            JR      Z, SD2DOS_WROK
            LD      C, A
            CALL    FERROR
            JP      COPY_ABORT_SD
SD2DOS_WROK
            INC     H
SD2DOS_WSTRT
            DJNZ    SD2DOS_WLOOP
            POP     BC
            LD      A, C
            AND     A
            JR      NZ, SD2DOS_DONE
            CP      B
            JR      NZ, SD2DOS_LOOP

SD2DOS_DONE
            LD      A, (OPT_VERIFY)
            OR      A
            CALL    NZ, CRC_SAVE_SOURCE
            CALL    SD_CLOSE
            JP      NZ, ERR_SDIO_CLOSE_DST
            LD      A, C
            LD      (DST_DCB + DCB_EOF), A
            CALL    DST_SETERN
            LD      DE, DST_DCB
            CALL    FCLOSE
            JP      NZ, ERR_CLOSE
            CALL    VERIFY_DOS_DEST
            JP      COPY_SUCCESS

COPY_DOS_TO_SD
            CALL    SD_PRESENT
            JP      NZ, ERR_NOSD

            LD      HL, DST_NAME + 3
            CALL    BUILD_SD_DST_PATH
            JP      NZ, ERR_SDNAME
            LD      HL, SD_NAMEBUF
            LD      (SD_PATHPTR), HL

            CALL    DEST_EXISTS_SD      ; check if SD dest already exists
            JR      Z, DOS2SD_OPEN      ; Z = proceed
            LD      A, (BATCH_MODE)
            OR      A
            RET     NZ                  ; in batch (wildcard): return to loop
            JP      FEXIT               ; single file: exit

DOS2SD_OPEN
            LD      HL, (SRCPTR)
            LD      DE, SRC_DCB
            CALL    FSPEC
            JP      NZ, ERR_SPEC

            LD      HL, IOBUF
            LD      B, 0
            LD      DE, SRC_DCB
            CALL    FOPEN
            JR      Z, DOS2SD_OPEN_OK
            CP      42
            JP      NZ, ERR_OPEN_SRC
DOS2SD_OPEN_OK
            CALL    SRC_INIT_RECS
            LD      A, (OPT_VERIFY)
            OR      A
            CALL    NZ, CRC_RESET

            LD      DE, SRC_DCB
            CALL    FREAD
            JR      Z, DOS2SD_HAVE_DATA
            CP      FERR_EOF
            JR      Z, DOS2SD_EMPTY
            CP      FERR_RNG
            JR      Z, DOS2SD_EMPTY
            LD      C, A
            CALL    FERROR
            JP      COPY_ABORT_SRC

DOS2SD_HAVE_DATA
            CALL    SRC_DEC_RECS
            LD      A, (OPT_VERIFY)
            OR      A
            JR      Z, DOS2SD_HAVE_OPEN
            CALL    SRC_REC_BYTES
            CALL    CRC_UPDATE_IOBUF_BC
DOS2SD_HAVE_OPEN
            LD      HL, (SD_PATHPTR)
            CALL    SD_OPEN_WR
            JP      NZ, ERR_OPEN_SDDST_CLOSE_SRC
            JP      DOS2SD_WRITE

DOS2SD_EMPTY
            LD      HL, (SD_PATHPTR)
            CALL    SD_OPEN_WR
            JP      NZ, ERR_OPEN_SDDST_CLOSE_SRC
            LD      A, (OPT_VERIFY)
            OR      A
            CALL    NZ, CRC_SAVE_SOURCE
            CALL    SD_CLOSE
            JP      NZ, ERR_SDIO_CLOSE_SRC
            LD      DE, SRC_DCB
            CALL    FCLOSE
            CALL    VERIFY_SD_DEST
            JP      COPY_SUCCESS

; *** COPY_DOS_WILD_TO_SD and BUILD_SD_DST_FROM_DOS are at end of file ***

DOS2SD_LOOP
            LD      DE, SRC_DCB
            CALL    FREAD
            JR      Z, DOS2SD_HAVE_NEXT
            CP      FERR_EOF
            JR      Z, DOS2SD_DONE
            CP      FERR_RNG
            JR      Z, DOS2SD_DONE
            LD      C, A
            CALL    FERROR
            JP      COPY_ABORT_SD_SRC

DOS2SD_HAVE_NEXT
            CALL    SRC_DEC_RECS
            LD      A, (OPT_VERIFY)
            OR      A
            JR      Z, DOS2SD_WRITE
            CALL    SRC_REC_BYTES
            CALL    CRC_UPDATE_IOBUF_BC

DOS2SD_WRITE
            CALL    SRC_REC_COUNT
            CALL    SD_WRITE_IOBUF
            JR      Z, DOS2SD_LOOP
            JP      ERR_SDIO_CLOSE_SRCSD

DOS2SD_DONE
            LD      A, (OPT_VERIFY)
            OR      A
            CALL    NZ, CRC_SAVE_SOURCE
            CALL    SD_CLOSE
            JP      NZ, ERR_SDIO_CLOSE_SRC
            LD      DE, SRC_DCB
            CALL    FCLOSE
            JP      NZ, ERR_CLOSE
            CALL    VERIFY_SD_DEST
            JP      COPY_SUCCESS

; ----------------------------------------------------------------------
; LS-DOS wildcard -> SD copy
; e.g. LPPIP */ASC SD:  or  LPPIP FREH*/ASC SD:books/
; ----------------------------------------------------------------------

COPY_DOS_WILD_TO_SD
            LD      A, (MODELID)
            CP      4
            JR      Z, CDWSD_OK
            LD      HL, MSG_ERR_WILDM4
            CALL    DSPLY
            CALL    FEXIT
CDWSD_OK
            CALL    SD_PRESENT
            JP      NZ, ERR_NOSD

            LD      HL, SRC_NAME
            LD      DE, SRC_TMPL
            CALL    COPYCRSTR
            LD      HL, DST_NAME
            LD      DE, DST_TMPL
            CALL    COPYCRSTR

            LD      HL, SRC_TMPL
            CALL    PREP_DOS_WILD
            JP      NZ, ERR_SPEC

            LD      HL, SRC_TMPL
            CALL    RESOLVE_DOS_WILD_DRIVE
            JP      NZ, ERR_SPEC

            XOR     A
            LD      (WILD_COUNT), A
            LD      (WILD_COUNT + 1), A
            LD      A, 1
            LD      (BATCH_MODE), A

            CALL    ENUM_DOS_DIR        ; sets DOS_WRAP (0 = no wrap = scan all 256)

            XOR     A
            LD      (DOS_DECNUM), A

CDWSD_LOOP
            ; Stop at wrap point. DOS_WRAP=0 means no wrap found - use natural
            ; 255->0 rollover to end the loop.
            LD      A, (DOS_WRAP)
            OR      A
            JR      Z, CDWSD_SCAN
            LD      HL, DOS_WRAP
            LD      A, (DOS_DECNUM)
            CP      (HL)
            JR      Z, CDWSD_DONE_CHECK

CDWSD_SCAN
            LD      A, (DOS_DECNUM)
            LD      B, A
            LD      A, (DOS_DRVNO)
            LD      C, A
            CALL    FDIRRD
            JR      NZ, CDWSD_NEXT

            LD      A, (HL)
            AND     0D8H
            XOR     10H
            JR      NZ, CDWSD_NEXT

            PUSH    HL
            CALL    DOS_DIR_MATCH
            POP     HL
            JR      NZ, CDWSD_NEXT

            PUSH    HL
            CALL    DOS_BUILD_MATCH
            POP     HL
            JR      NZ, CDWSD_NEXT
            ; V0.1.69: same blank-entry guard as CDW_LOOP -- see lppip_dos_wild.inc
            LD      A, (DOS_MATCHBUF)   ; skip blank/deleted entries (all-spaces name)
            CP      CR
            JR      Z, CDWSD_NEXT

            CALL    BUILD_DOS_SOURCE_MATCH
            CALL    BUILD_SD_DST_FROM_DOS

            LD      HL, SRC_NAME
            LD      (SRCPTR), HL
            LD      HL, DST_NAME
            LD      (DSTPTR), HL
            CALL    SHOW_COPY_NAMES
            CALL    COPY_DOS_TO_SD

            LD      HL, (WILD_COUNT)
            INC     HL
            LD      (WILD_COUNT), HL

CDWSD_NEXT
            ; V0.1.68: ERR_SRC_BATCH now dispatches here (not CDW_NEXT) when DST_IS_SD is set.
            ; Jumping to CDW_NEXT here caused BUILD_DOS_DST_LOCAL to reject the SD destination
            ; on every entry, silently skipping all files (WILD_COUNT stayed 0 = NO FILE(S) FOUND).
            LD      HL, DOS_DECNUM
            INC     (HL)
            JR      NZ, CDWSD_LOOP      ; loop until 255->0 natural wrap

CDWSD_DONE_CHECK
            XOR     A
            LD      (BATCH_MODE), A
            LD      HL, (WILD_COUNT)
            LD      A, H
            OR      L
            JP      NZ, CDWSD_EXIT
            LD      HL, MSG_NO_DOSFILES
            CALL    DSPLY
CDWSD_EXIT
            CALL    FEXIT

; ----------------------------------------------------------------------
; BUILD_SD_DST_FROM_DOS
; Input:  DOS_MATCHBUF = matched LS-DOS name  e.g. "NARNIA1/ASC\r"
;         DST_TMPL     = SD template          e.g. "SD:\r" or "SD:books/\r"
; Output: DST_NAME     = full SD path         e.g. "SD:NARNIA1.ASC\r"
; LS-DOS '/' ext separator converted to '.' for SD/FAT naming.
; ----------------------------------------------------------------------

BUILD_SD_DST_FROM_DOS
            LD      HL, DST_TMPL
            LD      DE, DST_NAME
            LD      B, ':'          ; if prefix is empty, treat as bare SD:
BSDF_PREFIX
            LD      A, (HL)
            CP      CR
            JR      Z, BSDF_SEP
            LD      B, A            ; track last char written
            LD      (DE), A
            INC     HL
            INC     DE
            JR      BSDF_PREFIX
BSDF_SEP
            ; Insert '/' if path does not already end with ':' or '/'
            LD      A, B
            CP      ':'
            JR      Z, BSDF_FNAME
            CP      '/'
            JR      Z, BSDF_FNAME
            LD      A, '/'
            LD      (DE), A
            INC     DE
BSDF_FNAME
            LD      HL, DOS_MATCHBUF
BSDF_COPY
            LD      A, (HL)
            CP      CR
            JR      Z, BSDF_DONE
            CP      '/'
            JR      NZ, BSDF_NOCONV
            LD      A, '.'
BSDF_NOCONV
            LD      (DE), A
            INC     HL
            INC     DE
            JR      BSDF_COPY
BSDF_DONE
            LD      A, CR
            LD      (DE), A
            XOR     A
            RET

; ----------------------------------------------------------------------
; EXPAND_SD_BARE_DST
; If DST_NAME is a bare ':digit' spec, expand it using the SD source
; basename from SRC_NAME+3 (the part after "SD:").
; Converts SD 8.3 naming to LS-DOS: "NIALL.DAT" -> "NIALL/DAT:4"
; Skips directory prefixes in the SD path (uses part after last '/').
; If DST_NAME is not bare ':digit', returns immediately (no change).
; ----------------------------------------------------------------------

EXPAND_SD_BARE_DST
            LD      A, (DST_NAME)
            CP      ':'
            RET     NZ
            LD      A, (DST_NAME + 1)
            CP      '0'
            RET     C
            CP      '8'
            RET     NC
            LD      B, A            ; B = drive digit

            ; Find SD basename: SRC_NAME+3 after the last '/'.
            LD      HL, SRC_NAME + 3
            LD      DE, SRC_NAME + 3    ; default: no slashes
ESBD_SCAN
            LD      A, (HL)
            CP      CR
            JR      Z, ESBD_BUILD
            CP      '/'
            JR      NZ, ESBD_NEXT
            INC     HL
            LD      D, H
            LD      E, L            ; DE = position after last '/'
            JR      ESBD_SCAN
ESBD_NEXT
            INC     HL
            JR      ESBD_SCAN

ESBD_BUILD
            ; DE = start of SD basename e.g. "NIALL.DAT"
            ; Build into DST_NAME: convert '.' to '/', append ':digit'
            LD      HL, DST_NAME
ESBD_NAME
            LD      A, (DE)
            CP      CR
            JR      Z, ESBD_NODOT   ; no extension
            CP      '.'
            JR      Z, ESBD_DOT
            LD      (HL), A
            INC     DE
            INC     HL
            JR      ESBD_NAME
ESBD_DOT
            LD      A, '/'
            LD      (HL), A
            INC     HL
            INC     DE
ESBD_EXT
            LD      A, (DE)
            CP      CR
            JR      Z, ESBD_NODOT
            LD      (HL), A
            INC     DE
            INC     HL
            JR      ESBD_EXT
ESBD_NODOT
            LD      A, ':'
            LD      (HL), A
            INC     HL
            LD      A, B            ; drive digit
            LD      (HL), A
            INC     HL
            LD      A, CR
            LD      (HL), A
            LD      HL, DST_NAME
            LD      (DSTPTR), HL
            RET
