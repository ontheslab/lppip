; ----------------------------------------------------------------------
; Shared FreHD low-level helpers
; ----------------------------------------------------------------------

SD_PRESENT
            LD      A, SD_CMD_GETVER
            OUT     (COMMAND2), A
            CALL    SD_WAIT_SILENT
            RET     NZ
            IN      A, (SIZE2)
            CP      6
            RET

SD_WAIT_SILENT
            ; CAUTION: SD_WAIT_BUSYCHK resets B to 0. Any caller that stores a
            ; value in B before calling SD_WAIT_SILENT must save it to D first
            ; and restore after. (V0.1.55: path length in B was clobbered here,
            ; causing OPENDIR to try to open a 0-byte path -- "NO SD FILE(S) FOUND".)
            EX      (SP), HL
            EX      (SP), HL
            LD      B, 0
SD_WAIT_LOOP
            IN      A, (STATUS)
            CP      0FFH
            JR      NZ, SD_WAIT_BUSYCHK
            INC     B
            LD      A, B
            CP      4
            JR      NC, SD_WAIT_NODEV
            JR      SD_WAIT_LOOP
SD_WAIT_BUSYCHK
            LD      B, 0            ; resets B -- see caution above
            LD      (SD_STATUSB), A
            ; Busy-bit test from FreHD reference (export2.z80). RLCA rotates bit7 into carry.
            ; V0.1.24: early code used JR NZ instead of JR C -- wrong on Z80 for RLCA result.
            RLCA
            JR      C, SD_WAIT_LOOP
            IN      A, (STATUS)
            LD      (SD_STATUSB), A
            BIT     0, A
            JR      NZ, SD_WAIT_ERR
            XOR     A
            LD      (SD_ERRCODE), A
            RET
SD_WAIT_ERR
            IN      A, (ERROR2)
            LD      (SD_ERRCODE), A
            OR      1
            RET
SD_WAIT_NODEV
            LD      A, 0FFH
            LD      (SD_ERRCODE), A
            OR      1
            RET

SD_BUILD_OPENBUF
            LD      DE, SD_OPENBUF
            LD      (DE), A
            INC     DE
            LD      B, 2
SD_BUILD_LOOP
            LD      A, (HL)
            CP      CR
            JR      Z, SD_BUILD_DONE
            CALL    UPCASE_A
            LD      (DE), A
            INC     HL
            INC     DE
            INC     B
            JR      SD_BUILD_LOOP
SD_BUILD_DONE
            XOR     A
            LD      (DE), A
            LD      A, B
            LD      (SD_OPENLEN), A
            RET

SD_OPEN_RD
            LD      A, SD_FA_READ
            CALL    SD_BUILD_OPENBUF
            LD      A, SD_CMD_OPENFILE
            OUT     (COMMAND2), A
            CALL    SD_WAIT_SILENT
            RET     NZ
            LD      A, (SD_OPENLEN)
            OUT     (SIZE2), A
            LD      B, A
            LD      C, DATA2
            LD      HL, SD_OPENBUF
            OTIR
            JP      SD_WAIT_SILENT

SD_OPEN_WR
            LD      A, SD_FA_WRITE + SD_FA_CREATE_ALWAYS
            CALL    SD_BUILD_OPENBUF
            LD      A, SD_CMD_OPENFILE
            OUT     (COMMAND2), A
            CALL    SD_WAIT_SILENT
            RET     NZ
            LD      A, (SD_OPENLEN)
            OUT     (SIZE2), A
            LD      B, A
            LD      C, DATA2
            LD      HL, SD_OPENBUF
            OTIR
            JP      SD_WAIT_SILENT

SD_CLOSE
            LD      A, SD_CMD_CLOSEFILE
            OUT     (COMMAND2), A
            JP      SD_WAIT_SILENT

SD_READ_BLOCK
            ; Read pattern from FreHD import2 reference (import2_ldos.z80).
            XOR     A
            OUT     (SIZE2), A
            LD      A, SD_CMD_READFILE
            OUT     (COMMAND2), A
            CALL    SD_WAIT_SILENT
            JR      NZ, SD_READ_ERR
            IN      A, (STATUS)
            LD      (SD_STATUSB), A
            AND     08H
            JR      Z, SD_READ_EOF

            LD      HL, IOBUF
            LD      BC, 256
            CALL    CLEAR_MEM

            IN      A, (SIZE2)
            LD      (SD_LASTEOF), A
            OR      A
            JR      NZ, SD_READ_HAVECOUNT
            LD      B, 0
            JR      SD_READ_BYTES
SD_READ_HAVECOUNT
            LD      B, A
SD_READ_BYTES
            LD      HL, IOBUF
            LD      C, DATA2
            ; V0.1.26: use INIR from import2 reference. Early version used a hand-rolled
            ; IN A,(DATA2) loop which corrupted the receive buffer (sparse byte sampling).
            INIR
SD_READ_OK
            XOR     A
            RET
SD_READ_EOF
            XOR     A
            LD      (SD_LASTEOF), A
            LD      A, 1
            OR      A
            RET
SD_READ_ERR
            LD      A, 2
            OR      A
            RET

; Read one FreHD block and return the original import2-style count in BC:
; Based directly on the import2_ldos.z80 reference read loop.
; BC=0000 means EOF/no data, BC=0100 means 256 bytes, BC=00nn means n bytes.
SD_READ_COUNT
            XOR     A
            OUT     (SIZE2), A
            LD      A, SD_CMD_READFILE
            OUT     (COMMAND2), A
            CALL    SD_WAIT_SILENT
            JR      NZ, SD_READCNT_ERR
            LD      BC, 0000H
            IN      A, (STATUS)
            LD      (SD_STATUSB), A
            AND     08H
            JR      Z, SD_READCNT_OK

            LD      HL, IOBUF
            LD      BC, 256
            CALL    CLEAR_MEM

            IN      A, (SIZE2)
            LD      C, A
            OR      A
            JR      NZ, SD_READCNT_HAVE
            LD      B, 1
SD_READCNT_HAVE
            PUSH    BC
            LD      B, C
            LD      C, DATA2
            LD      HL, IOBUF
            INIR
            POP     BC
SD_READCNT_OK
            XOR     A
            RET
SD_READCNT_ERR
            LD      A, 1
            OR      A
            RET

SD_WRITE_IOBUF
            PUSH    AF
            LD      A, SD_CMD_WRITEFILE
            OUT     (COMMAND2), A
            CALL    SD_WAIT_SILENT
            JR      NZ, SD_WRITE_FAIL_POP
            POP     AF
            OUT     (SIZE2), A
            OR      A
            JR      NZ, SD_WRITE_HAVECOUNT
            LD      BC, 256
            JR      SD_WRITE_BYTES
SD_WRITE_HAVECOUNT
            LD      C, A
            LD      B, 0
SD_WRITE_BYTES
            LD      HL, IOBUF
SD_WRITE_LOOP
            LD      A, B
            OR      C
            JR      Z, SD_WRITE_WAIT
            LD      A, (HL)
            OUT     (DATA2), A
            INC     HL
            DEC     BC
            JR      SD_WRITE_LOOP
SD_WRITE_WAIT
            JP      SD_WAIT_SILENT
SD_WRITE_FAIL_POP
            POP     AF
            RET

; SRC_INIT_RECS / SRC_DEC_RECS / SRC_REC_COUNT:
;   Remaining-record count model from export2.z80 reference.
;   V0.1.25: replaced a NRN==ERN heuristic that wrote a full 256-byte final block
;   instead of the real EOF byte count, producing a garbage tail on re-import.
SRC_INIT_RECS
            LD      HL, (SRC_DCB + DCB_ERN)
            LD      (SRC_REMAIN), HL
            RET

SRC_DEC_RECS
            LD      HL, (SRC_REMAIN)
            LD      A, H
            OR      L
            RET     Z
            DEC     HL
            LD      (SRC_REMAIN), HL
            RET

SRC_REC_COUNT
            LD      HL, (SRC_REMAIN)
            LD      A, H
            OR      L
            JR      Z, SRC_REC_LAST
            XOR     A
            RET
SRC_REC_LAST
            LD      A, (SRC_DCB + DCB_EOF)
            RET

DST_SETERN
            LD      HL, (DST_DCB + DCB_NRN)
            LD      (DST_DCB + DCB_ERN), HL
            RET

BUILD_SD_DST_PATH
            LD      DE, SD_NAMEBUF
            LD      B, 0
BSD_COPY
            LD      A, (HL)
            CP      CR
            JR      Z, BSD_APPEND_CHECK
            CALL    UPCASE_A
            LD      (DE), A
            LD      B, A
            INC     HL
            INC     DE
            JR      BSD_COPY
BSD_APPEND_CHECK
            LD      A, B
            OR      A
            JR      Z, BSD_APPEND_NAME
            CP      '/'
            JR      Z, BSD_APPEND_NAME
            JR      BSD_TERM
BSD_APPEND_NAME
            LD      HL, SRC_NAME
            CALL    APPEND_DOS_FILENAME
            RET     NZ
BSD_TERM
            LD      A, CR
            LD      (DE), A
            XOR     A
            RET

APPEND_DOS_FILENAME
            LD      B, 0
ADF_NAME
            LD      A, (HL)
            CP      ':'
            JR      Z, ADF_NAME_DONE
            CP      CR
            JR      Z, ADF_NAME_DONE
            CP      '/'
            JR      Z, ADF_EXTCHK
            CALL    UPCASE_A
            LD      (DE), A
            INC     HL
            INC     DE
            INC     B
            JR      ADF_NAME
ADF_NAME_DONE
            LD      A, B
            OR      A
            JR      Z, ADF_FAIL
            XOR     A
            RET
ADF_EXTCHK
            LD      A, B
            OR      A
            JR      Z, ADF_FAIL
            INC     HL
            LD      A, (HL)
            CP      ':'
            JR      Z, ADF_OK
            CP      CR
            JR      Z, ADF_OK
            LD      A, '.'
            LD      (DE), A
            INC     DE
ADF_EXT
            LD      A, (HL)
            CP      ':'
            JR      Z, ADF_OK
            CP      CR
            JR      Z, ADF_OK
            CALL    UPCASE_A
            LD      (DE), A
            INC     HL
            INC     DE
            JR      ADF_EXT
ADF_OK
            XOR     A
            RET
ADF_FAIL
            LD      A, 1
            OR      A
            RET

PRINT_SD_PATH
            PUSH    HL
            LD      HL, MSG_SD_PREFIX_Z
            CALL    PSTRZ
            POP     HL
PRINT_SD_LOOP
            LD      A, (HL)
            CP      CR
            RET     Z
            OR      A
            RET     Z
            CALL    DSP
            INC     HL
            JR      PRINT_SD_LOOP

SHOW_SD_DIAG
            LD      HL, MSG_ERR_SDIO0
            CALL    PSTRZ
            LD      HL, MSG_ERR_SDIO1
            CALL    PSTRZ
            LD      A, (SD_STATUSB)
            CALL    PRHEX8
            LD      HL, MSG_ERR_SDIO2
            CALL    PSTRZ
            LD      A, (SD_ERRCODE)
            CALL    PRHEX8
            LD      A, CR
            CALL    DSP
            RET

PREP_SD_WILD
            LD      HL, SRC_TMPL + 3
            LD      (SD_PATPTR), HL
PSW_SCAN
            LD      A, (HL)
            CP      CR
            JR      Z, PSW_SPLIT
            CP      '/'
            JR      Z, PSW_MARK
            CP      '\'
            JR      Z, PSW_MARK
            INC     HL
            JR      PSW_SCAN
PSW_MARK
            INC     HL
            LD      (SD_PATPTR), HL
            JR      PSW_SCAN

PSW_SPLIT
            LD      HL, SRC_TMPL + 3
            LD      DE, SD_DIRBUF
            LD      BC, (SD_PATPTR)
PSW_DIR
            LD      A, H
            CP      B
            JR      NZ, PSW_DIRCPY
            LD      A, L
            CP      C
            JR      Z, PSW_DIREND
PSW_DIRCPY
            LD      A, (HL)
            CALL    UPCASE_A
            LD      (DE), A
            INC     HL
            INC     DE
            JR      PSW_DIR
PSW_DIREND
            LD      A, CR
            LD      (DE), A

            LD      HL, (SD_PATPTR)
            LD      DE, SD_WILDBUF
PSW_WILD
            LD      A, (HL)
            CP      CR
            JR      Z, PSW_WTERM
            CALL    UPCASE_A
PSW_WTERM
            LD      (DE), A
            INC     HL
            INC     DE
            CP      CR
            JR      NZ, PSW_WILD
            RET

SD_OPEN_DIRBUF
            LD      HL, SD_DIRBUF
            LD      B, 0
            LD      D, 0
SOD_COUNT
            LD      A, (HL)
            CP      CR
            JR      Z, SOD_CEND
            LD      D, A
            INC     B
            INC     HL
            JR      SOD_COUNT
SOD_CEND
            LD      A, D
            CP      '/'
            JR      Z, SOD_STRIP
            CP      '\'
            JR      NZ, SOD_READY
SOD_STRIP
            LD      A, B
            OR      A
            JR      Z, SOD_READY
            DEC     B
SOD_READY
            LD      A, B
            OR      A
            JR      Z, SOD_ROOT
            INC     A
            OUT     (SIZE2), A
            LD      A, SD_CMD_OPENDIR
            OUT     (COMMAND2), A
            LD      D, B            ; V0.1.55: save B before SD_WAIT_SILENT (resets B at SD_WAIT_BUSYCHK)
            CALL    SD_WAIT_SILENT
            LD      B, D            ; restore path length
            RET     NZ
            LD      HL, SD_DIRBUF
            LD      C, DATA2
SOD_SEND
            LD      A, B
            OR      A
            JR      Z, SOD_TERM
            LD      A, (HL)
            OUT     (C), A
            INC     HL
            DEC     B
            JR      SOD_SEND
SOD_TERM
            XOR     A
            OUT     (C), A
            JP      SD_WAIT_SILENT
SOD_ROOT
            LD      A, 2
            OUT     (SIZE2), A
            LD      A, SD_CMD_OPENDIR
            OUT     (COMMAND2), A
            CALL    SD_WAIT_SILENT
            RET     NZ
            LD      A, '/'
            OUT     (DATA2), A
            XOR     A
            OUT     (DATA2), A
            JP      SD_WAIT_SILENT

SD_OPEN_DIRBUF_ABS
            LD      HL, SD_DIRBUF
            LD      B, 0
            LD      D, 0
SOA_COUNT
            LD      A, (HL)
            CP      CR
            JR      Z, SOA_CEND
            LD      D, A
            INC     B
            INC     HL
            JR      SOA_COUNT
SOA_CEND
            LD      A, D
            CP      '/'
            JR      Z, SOA_STRIP
            CP      '\'
            JR      NZ, SOA_READY
SOA_STRIP
            LD      A, B
            OR      A
            JR      Z, SOA_READY
            DEC     B
SOA_READY
            LD      A, B
            ADD     A, 2           ; leading '/' plus NUL
            OUT     (SIZE2), A
            LD      A, SD_CMD_OPENDIR
            OUT     (COMMAND2), A
            LD      D, B            ; V0.1.55: save B before SD_WAIT_SILENT (resets B at SD_WAIT_BUSYCHK)
            CALL    SD_WAIT_SILENT
            LD      B, D            ; restore path length
            RET     NZ
            LD      C, DATA2
            LD      A, '/'
            OUT     (C), A
            LD      HL, SD_DIRBUF
SOA_SEND
            LD      A, B
            OR      A
            JR      Z, SOA_TERM
            LD      A, (HL)
            OUT     (C), A
            INC     HL
            DEC     B
            JR      SOA_SEND
SOA_TERM
            XOR     A
            OUT     (C), A
            JP      SD_WAIT_SILENT

SD_FNMATCH
            LD      (SD_PATPTR), HL
            LD      (SD_STRPTR), DE
            XOR     A
            LD      (SD_PATSTAR), A
            LD      (SD_PATSTAR + 1), A
            LD      (SD_STRSTAR), A
            LD      (SD_STRSTAR + 1), A
SFM_LOOP
            LD      HL, (SD_STRPTR)
            LD      A, (HL)
            CALL    UPCASE_A        ; case-insensitive match: upcase string char
            LD      B, A
            OR      A
            JR      Z, SFM_ENDSTR

            LD      HL, (SD_PATPTR)
            LD      A, (HL)
            CP      CR
            JR      NZ, SFM_PATOK
            XOR     A
SFM_PATOK
            CP      '*'
            JR      Z, SFM_STAR
            CP      '?'
            JR      Z, SFM_MATCH
            CP      B
            JR      Z, SFM_MATCH

            LD      HL, (SD_PATSTAR)
            LD      A, H
            OR      L
            JR      Z, SFM_NOMATCH
            INC     HL
            LD      (SD_PATPTR), HL
            LD      HL, (SD_STRSTAR)
            INC     HL
            LD      (SD_STRSTAR), HL
            LD      (SD_STRPTR), HL
            JR      SFM_LOOP

SFM_STAR
            LD      HL, (SD_PATPTR)
            LD      (SD_PATSTAR), HL
            INC     HL
            LD      (SD_PATPTR), HL
            LD      HL, (SD_STRPTR)
            LD      (SD_STRSTAR), HL
            JR      SFM_LOOP

SFM_MATCH
            LD      HL, (SD_PATPTR)
            INC     HL
            LD      (SD_PATPTR), HL
            LD      HL, (SD_STRPTR)
            INC     HL
            LD      (SD_STRPTR), HL
            JR      SFM_LOOP

SFM_ENDSTR
            LD      HL, (SD_PATPTR)
SFM_SKIP
            LD      A, (HL)
            CP      '*'
            JR      NZ, SFM_DONE
            INC     HL
            JR      SFM_SKIP
SFM_DONE
            CP      CR
            JR      Z, SFM_YES
            OR      A
            JR      Z, SFM_YES
SFM_NOMATCH
            XOR     A
            RET
SFM_YES
            LD      A, 1
            RET

SD_READDIR_MATCH
SRDM_NEXT
            LD      A, SD_CMD_READDIR
            OUT     (COMMAND2), A
            CALL    SD_WAIT_SILENT
            JR      NZ, SRDM_ERR
            LD      A, (SD_STATUSB)
            AND     08H
            JR      Z, SRDM_DONE

            LD      HL, SD_FINFO
            LD      BC, FINFO_SIZE
            CALL    CLEAR_MEM

            IN      A, (SIZE2)
            CP      FINFO_SIZE
            JR      C, SRDM_SIZEOK
            LD      A, FINFO_SIZE
SRDM_SIZEOK
            LD      B, A
            LD      C, DATA2
            LD      HL, SD_FINFO
            INIR

            LD      A, (SD_FINFO + FINFO_ATTR)
            AND     FA_DIR + FA_VOL + FA_HID
            JR      NZ, SRDM_NEXT

            LD      HL, SD_WILDBUF
            LD      DE, SD_FINFO + FINFO_NAME
            CALL    SD_FNMATCH
            OR      A
            JR      Z, SRDM_NEXT

            LD      HL, SD_FINFO + FINFO_NAME
            LD      DE, SD_MATCHBUF
SRDM_COPY
            LD      A, (HL)
            OR      A
            JR      Z, SRDM_CEND
            LD      (DE), A
            INC     HL
            INC     DE
            JR      SRDM_COPY
SRDM_CEND
            LD      A, CR
            LD      (DE), A
            LD      A, 1
            OR      A
            RET
SRDM_DONE
            XOR     A
            RET
SRDM_ERR
            SCF
            RET

SD_STORE_MATCH
            LD      HL, (WILD_COUNT)
            LD      A, H
            OR      A
            RET     NZ
            LD      A, L
            CP      SD_WILD_MAX
            RET     NC
            LD      HL, SD_MATCHTAB
            LD      A, (WILD_COUNT)
            LD      C, A
SSM_OFS
            LD      A, C
            JR      Z, SSM_COPY
            LD      A, SD_MATCH_LEN
            ADD     A, L
            LD      L, A
            JR      NC, SSM_NOC
            INC     H
SSM_NOC
            DEC     C
            JR      SSM_OFS
SSM_COPY
            LD      DE, SD_MATCHBUF
            LD      B, SD_MATCH_LEN
SSM_CL
            LD      A, (DE)
            LD      (HL), A
            INC     DE
            INC     HL
            DJNZ    SSM_CL
            LD      HL, (WILD_COUNT)
            INC     HL
            LD      (WILD_COUNT), HL
            XOR     A
            RET

SD_LOAD_MATCH
            LD      HL, SD_MATCHTAB
            LD      A, (WILD_INDEX)
            LD      C, A
SLM_OFS
            LD      A, C
            JR      Z, SLM_COPY
            LD      A, SD_MATCH_LEN
            ADD     A, L
            LD      L, A
            JR      NC, SLM_NOC
            INC     H
SLM_NOC
            DEC     C
            JR      SLM_OFS
SLM_COPY
            LD      DE, SD_MATCHBUF
            LD      B, SD_MATCH_LEN
SLM_CL
            LD      A, (HL)
            LD      (DE), A
            INC     DE
            INC     HL
            DJNZ    SLM_CL
            RET

SD_ENUM_WILD
            XOR     A
            LD      (WILD_COUNT), A
            LD      (WILD_COUNT + 1), A
            CALL    SD_OPEN_DIRBUF
            JR      Z, SEW_PASS1
            SCF
            RET
SEW_PASS1
            CALL    SD_ENUM_WILD_LOOP
            RET     C
            LD      HL, (WILD_COUNT)
            LD      A, H
            OR      L
            RET     NZ
            LD      A, (SD_DIRBUF)
            CP      CR
            RET     Z
            CALL    SD_OPEN_DIRBUF_ABS
            JR      Z, SEW_PASS2
            SCF
            RET
SEW_PASS2
            CALL    SD_ENUM_WILD_LOOP
            RET

SD_ENUM_WILD_LOOP
            CALL    SD_READDIR_MATCH
            RET     C
            OR      A
            RET     Z
            CALL    SD_STORE_MATCH
            JR      SD_ENUM_WILD_LOOP

BUILD_SD_SOURCE_MATCH
            LD      DE, SRC_NAME
            LD      A, 'S'
            LD      (DE), A
            INC     DE
            LD      A, 'D'
            LD      (DE), A
            INC     DE
            LD      A, ':'
            LD      (DE), A
            INC     DE
            LD      HL, SD_DIRBUF
BSS_PREFIX
            LD      A, (HL)
            CP      CR
            JR      Z, BSS_NAME
            LD      (DE), A
            INC     HL
            INC     DE
            JR      BSS_PREFIX
BSS_NAME
            LD      HL, SD_MATCHBUF
BSS_COPY
            LD      A, (HL)
            LD      (DE), A
            INC     HL
            INC     DE
            CP      CR
            JR      NZ, BSS_COPY
            RET

BUILD_DOS_DST_MATCH
            LD      HL, DST_TMPL
            LD      A, (HL)
            CP      ':'
            JR      Z, BDM_BARE
            LD      A, 1
            OR      A
            RET
BDM_BARE
            LD      DE, DST_NAME
            LD      HL, SD_MATCHBUF
            LD      B, 0
BDM_NAME
            LD      A, (HL)
            CP      '.'
            JR      Z, BDM_EXTCHK
            CP      CR
            JR      Z, BDM_NAMEEND
            LD      (DE), A
            INC     HL
            INC     DE
            INC     B
            JR      BDM_NAME
BDM_NAMEEND
            LD      A, B
            OR      A
            JR      Z, BDM_FAIL
            JR      BDM_DRIVE
BDM_EXTCHK
            LD      A, B
            OR      A
            JR      Z, BDM_FAIL
            LD      A, '/'
            LD      (DE), A
            INC     DE
            INC     HL
BDM_EXT
            LD      A, (HL)
            CP      CR
            JR      Z, BDM_DRIVE
            LD      (DE), A
            INC     HL
            INC     DE
            JR      BDM_EXT
BDM_DRIVE
            LD      HL, DST_TMPL
BDM_FINDCOL
            LD      A, (HL)
            CP      ':'
            JR      Z, BDM_COPYDRV
            CP      CR
            JR      Z, BDM_TAIL
            INC     HL
            JR      BDM_FINDCOL
BDM_COPYDRV
            LD      A, (HL)
            LD      (DE), A
            INC     HL
            INC     DE
            CP      CR
            JR      NZ, BDM_COPYDRV
BDM_TAIL
            LD      A, CR
            LD      (DE), A
            XOR     A
            RET
BDM_FAIL
            LD      A, 1
            OR      A
            RET

; SD_NAME_FITS_83
; Check that SD_MATCHBUF holds a valid 8.3 filename (1-8 char name, 0-3 char ext).
; Returns Z if it fits, NZ if the name or extension is too long.
; Preserves all registers except AF.
;
SD_NAME_FITS_83
            PUSH    HL
            PUSH    BC
            LD      HL, SD_MATCHBUF
            LD      B, 0            ; name char count
SN83_NAME
            LD      A, (HL)
            CP      CR
            JR      Z, SN83_OK      ; no extension, name-only
            CP      '.'
            JR      Z, SN83_EXT     ; start of extension
            INC     B
            INC     HL
            LD      A, B
            CP      9               ; 9 means > 8 chars
            JR      C, SN83_NAME    ; B < 9, keep going
            JR      SN83_FAIL       ; B >= 9, too long
SN83_EXT
            INC     HL              ; skip '.'
            LD      B, 0            ; ext char count
SN83_EXTLOOP
            LD      A, (HL)
            CP      CR
            JR      Z, SN83_OK
            INC     B
            INC     HL
            LD      A, B
            CP      4               ; 4 means > 3 chars
            JR      C, SN83_EXTLOOP ; B < 4, keep going
                                    ; fall through: too long
SN83_FAIL
            POP     BC
            POP     HL
            OR      1               ; NZ = too long
            RET
SN83_OK
            POP     BC
            POP     HL
            XOR     A               ; Z = fits
            RET

