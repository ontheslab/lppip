; ----------------------------------------------------------------------
; LS-DOS local wildcard wrapper
; ----------------------------------------------------------------------

COPY_DOS_WILD_TO_DOS
            LD      A, (MODELID)
            CP      4
            JR      Z, CDW_OK
            LD      HL, MSG_ERR_WILDM4
            CALL    DSPLY
            CALL    FEXIT
CDW_OK
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

            ; Scan DECs using ENUM_DOS_DIR which empirically detects the directory
            ; wrap point by finding when @DIRRD returns a previously-seen entry.
            ; No HIT sector, no @GTDCT, no fallback guessing needed.
            CALL    ENUM_DOS_DIR        ; sets DOS_WRAP (0 = no wrap = scan all 256)

            XOR     A
            LD      (DOS_DECNUM), A

CDW_LOOP
            ; Stop at wrap point. DOS_WRAP=0 means no wrap found - use natural
            ; 255->0 rollover to end the loop.
            LD      A, (DOS_WRAP)
            OR      A
            JR      Z, CDW_SCAN
            LD      HL, DOS_WRAP
            LD      A, (DOS_DECNUM)
            CP      (HL)
            JR      Z, CDW_LOOP_END

CDW_SCAN
            LD      A, (DOS_DECNUM)
            LD      B, A                ; B = DEC for @DIRRD
            LD      A, (DOS_DRVNO)
            LD      C, A                ; C = drive for @DIRRD
            CALL    FDIRRD
            JR      NZ, CDW_NEXT

            ; Attribute check from LS-DOS sys12.asm DOALL: passes only active FPDEs
            ; (bit4=1 = FPDE, bits 7/6/3=0 = not SYS/INV/FXDE)
            LD      A, (HL)
            AND     0D8H
            XOR     10H
            JR      NZ, CDW_NEXT

            PUSH    HL
            CALL    DOS_DIR_MATCH
            POP     HL
            JR      NZ, CDW_NEXT

            PUSH    HL
            CALL    DOS_BUILD_MATCH
            POP     HL
            JR      NZ, CDW_NEXT
            ; V0.1.69: stale/deleted entries with all-spaces name pass the attribute
            ; check and match */* -- DOS_BUILD_MATCH returns an empty buffer (just CR).
            ; Skip them to avoid INVALID OR MISSING SD: NAME abort at end of scan.
            LD      A, (DOS_MATCHBUF)   ; skip blank/deleted entries (all-spaces name)
            CP      CR
            JR      Z, CDW_NEXT

            CALL    BUILD_DOS_SOURCE_MATCH
            CALL    BUILD_DOS_DST_LOCAL
            JR      NZ, CDW_NEXT

            LD      HL, SRC_NAME
            LD      (SRCPTR), HL
            LD      HL, DST_NAME
            LD      (DSTPTR), HL
            CALL    SHOW_COPY_NAMES
            CALL    DEST_EXISTS_DOS
            JR      NZ, CDW_NEXT        ; skip: user said no or R/O without /W
            CALL    COPY_LOCAL

            LD      HL, (WILD_COUNT)
            INC     HL
            LD      (WILD_COUNT), HL

CDW_NEXT
            ; V0.1.43: DOS_DECNUM is a memory variable, not register E.
            ; Using E as the loop counter caused "NO FILE(S) FOUND" because
            ; DOS_DIR_MATCH does LD DE, DOS_WILDBUF which clobbered E on every non-match.
            LD      HL, DOS_DECNUM
            INC     (HL)                ; bump DEC counter
            JR      NZ, CDW_LOOP        ; all 256 DECs, wraps 255->0 to stop

CDW_LOOP_END

            XOR     A
            LD      (BATCH_MODE), A
            LD      HL, (WILD_COUNT)
            LD      A, H
            OR      L
            JR      NZ, CDW_DONE
            LD      HL, MSG_NO_DOSFILES
            CALL    DSPLY
CDW_DONE
            CALL    FEXIT

RESOLVE_DOS_WILD_DRIVE
            ; V0.1.44: @FSPEC (SVC 78) gave wrong results for specs with no drive --
            ; both 'A/ASC' and 'A/ASC:0' produced DOS_DRVNO=0, but hardware showed
            ; 'A/ASC' (no drive) failed to find files even when they were on drive :0.
            ; Fix: scan the token directly for ':digit'; fall back to LDRV$ at 0023H.
RDWD_SCAN
            LD      A, (HL)
            CP      CR
            JR      Z, RDWD_USELOGD     ; end of spec, no drive given
            CP      ':'
            JR      Z, RDWD_COLON
            INC     HL
            JR      RDWD_SCAN
RDWD_COLON
            INC     HL
            LD      A, (HL)
            CP      '0'
            JR      C, RDWD_USELOGD     ; char before '0', not a drive digit
            CP      '8'
            JR      NC, RDWD_USELOGD    ; '8' or above, not a valid '0'-'7'
            SUB     '0'                 ; ASCII digit to binary drive number
            AND     07H
            LD      (DOS_DRVNO), A
            XOR     A
            RET
RDWD_USELOGD
            LD      A, (023H)           ; LDRV$ - LS-DOS current logged drive
            AND     07H
            LD      (DOS_DRVNO), A
            XOR     A
            RET

; ----------------------------------------------------------------------
; ENUM_DOS_DIR
;   Detects the directory wrap point using @DIRRD only.
;   Reads DECs 0-255. Saves the first valid active file entry as SENTINEL.
;   When a subsequent @DIRRD returns the identical 32 bytes, the directory
;   has wrapped - that DEC is the wrap point.
;   Input:  DOS_DRVNO = source drive
;   Output: DOS_WRAP = wrap DEC (0 = no wrap found, scan all 256)
;   Destroys: AF, BC, DE, HL
;
;   Replaces the earlier HIT + @GTDCT6 approach (V0.1.58-63). @GTDCT6 (SVC 81)
;   failed for all drives on the test hardware, forcing a fallback that scanned
;   fixed numbers of DECs (16 or 256) and still produced duplicates on floppies.
;   Empirical wrap detection via SENTINEL comparison is drive-geometry-independent.
; ----------------------------------------------------------------------
ENUM_DOS_DIR
            XOR     A
            LD      (DOS_WRAP), A
            LD      (SENTINEL_FLAG), A
            LD      (DOS_DECNUM), A

ENUM_LOOP
            LD      A, (DOS_DECNUM)
            LD      B, A
            LD      A, (DOS_DRVNO)
            LD      C, A
            CALL    FDIRRD
            JR      NZ, ENUM_NEXT       ; @DIRRD error - skip this DEC

            ; Valid active file? (same check as CDW_LOOP)
            LD      A, (HL)
            AND     0D8H
            XOR     10H
            JR      NZ, ENUM_NEXT       ; not a valid file entry

            ; Check whether sentinel is set yet
            LD      A, (SENTINEL_FLAG)
            OR      A
            JR      NZ, ENUM_CHECK_WRAP

            ; First valid entry - save as sentinel (copy 32 bytes from OS buffer)
            LD      DE, SENTINEL
            LD      BC, 32
            LDIR
            LD      A, 1
            LD      (SENTINEL_FLAG), A
            JR      ENUM_NEXT

ENUM_CHECK_WRAP
            ; HL = current entry in OS buffer. Compare 32 bytes with SENTINEL.
            LD      DE, SENTINEL
            CALL    MEMCMP_32
            JR      NZ, ENUM_NEXT       ; different entry - no wrap yet

            ; Wrap detected: store current DEC as wrap point
            LD      A, (DOS_DECNUM)
            LD      (DOS_WRAP), A
            RET

ENUM_NEXT
            LD      HL, DOS_DECNUM
            INC     (HL)
            JR      NZ, ENUM_LOOP

            ; Scanned all 256 DECs with no wrap found
            XOR     A
            LD      (DOS_WRAP), A       ; 0 = no wrap, use natural loop end
            RET

; ----------------------------------------------------------------------
; MEMCMP_32
;   Compare 32 bytes.
;   Input:  HL = first buffer, DE = second buffer
;   Output: Z if equal, NZ if different
;   Destroys: AF, BC, DE, HL
; ----------------------------------------------------------------------
MEMCMP_32
            LD      B, 32
MCMP_LOOP   LD      A, (DE)
            CP      (HL)
            RET     NZ
            INC     HL
            INC     DE
            DJNZ    MCMP_LOOP
            RET                         ; Z set by final DJNZ (B decremented to 0)

PREP_DOS_WILD
            LD      DE, DOS_WILDBUF
            LD      B, 11
            LD      A, ' '
PDW_FILL
            LD      (DE), A
            INC     DE
            DJNZ    PDW_FILL

            LD      DE, DOS_WILDBUF
            LD      B, 8
            CALL    PARSE_DOS_WILD_FIELD
            RET     NZ

            LD      A, (HL)
            CP      '/'
            JR      Z, PDW_EXT
            CP      ':'
            JR      Z, PDW_OK
            CP      CR
            JR      Z, PDW_OK
            LD      A, 1
            OR      A
            RET
PDW_EXT
            INC     HL
            LD      DE, DOS_WILDBUF + 8
            LD      B, 3
            CALL    PARSE_DOS_WILD_FIELD
            RET     NZ
            LD      A, (HL)
            CP      ':'
            JR      Z, PDW_OK
            CP      CR
            JR      Z, PDW_OK
            LD      A, 1
            OR      A
            RET
PDW_OK
            XOR     A
            RET

PARSE_DOS_WILD_FIELD
PDWF_LOOP
            LD      A, (HL)
            CP      CR
            RET     Z
            CP      ':'
            RET     Z
            CP      '/'
            RET     Z
            CP      '*'
            JR      Z, PDWF_STAR
            CP      '?'
            JR      Z, PDWF_QM
            PUSH    BC
            CALL    UPCASE_A
            POP     BC
            LD      (DE), A
            INC     DE
            INC     HL
            DJNZ    PDWF_LOOP
            LD      A, (HL)
            CP      CR
            RET     Z
            CP      ':'
            RET     Z
            CP      '/'
            RET     Z
            LD      A, 1
            OR      A
            RET
PDWF_QM
            LD      (DE), A
            INC     DE
            INC     HL
            DJNZ    PDWF_LOOP
            RET
PDWF_STAR
            LD      A, '?'
PDWF_STARF
            LD      (DE), A
            INC     DE
            DJNZ    PDWF_STARF
            INC     HL
PDWF_SKIP
            LD      A, (HL)
            CP      CR
            RET     Z
            CP      ':'
            RET     Z
            CP      '/'
            RET     Z
            INC     HL
            JR      PDWF_SKIP

DOS_DIR_MATCH
            PUSH    HL
            LD      DE, 5
            ADD     HL, DE
            LD      DE, DOS_WILDBUF
            LD      B, 11
DDM_LOOP
            LD      A, (DE)
            CP      '?'
            JR      Z, DDM_NEXT
            CP      (HL)
            JR      NZ, DDM_FAIL
DDM_NEXT
            INC     DE
            INC     HL
            DJNZ    DDM_LOOP
            POP     HL
            XOR     A
            RET
DDM_FAIL
            POP     HL
            LD      A, 1
            OR      A
            RET

DOS_BUILD_MATCH
            PUSH    HL
            LD      DE, DOS_MATCHBUF
            LD      BC, 5
            ADD     HL, BC
            LD      B, 8
DBM_NAME
            LD      A, (HL)
            CP      ' '
            JR      Z, DBM_EXTCHK
            LD      (DE), A
            INC     DE
            INC     HL
            DJNZ    DBM_NAME
DBM_EXTCHK
            POP     HL
            PUSH    HL
            LD      BC, 13
            ADD     HL, BC
            LD      B, 3
            LD      A, (HL)
            CP      ' '
            JR      Z, DBM_DONE
            LD      A, '/'
            LD      (DE), A
            INC     DE
DBM_EXT
            LD      A, (HL)
            CP      ' '
            JR      Z, DBM_DONE
            LD      (DE), A
            INC     DE
            INC     HL
            DJNZ    DBM_EXT
DBM_DONE
            LD      A, CR
            LD      (DE), A
            POP     HL
            XOR     A
            RET

BUILD_DOS_SOURCE_MATCH
            LD      DE, SRC_NAME
            LD      HL, DOS_MATCHBUF
BDSM_COPY
            LD      A, (HL)
            CP      CR
            JR      Z, BDSM_DRIVE
            LD      (DE), A
            INC     DE
            INC     HL
            JR      BDSM_COPY
BDSM_DRIVE
            LD      A, ':'
            LD      (DE), A
            INC     DE
            LD      A, (DOS_DRVNO)
            OR      '0'
            LD      (DE), A
            INC     DE
            LD      A, CR
            LD      (DE), A
            RET

BUILD_DOS_DST_LOCAL
            LD      HL, DST_TMPL
            LD      A, (HL)
            CP      ':'
            JR      Z, BDDL_OK
            LD      A, 1
            OR      A
            RET
BDDL_OK
            LD      DE, DST_NAME
            LD      HL, DOS_MATCHBUF
BDDL_NAME
            LD      A, (HL)
            CP      CR
            JR      Z, BDDL_DRV
            LD      (DE), A
            INC     DE
            INC     HL
            JR      BDDL_NAME
BDDL_DRV
            LD      HL, DST_TMPL
BDDL_COPY
            LD      A, (HL)
            LD      (DE), A
            INC     DE
            INC     HL
            CP      CR
            JR      NZ, BDDL_COPY
            XOR     A
            RET

FDIRRD
            ; @DIRRD6 (SVC 87): input B=DEC, C=drive; output HL -> OS directory buffer.
            ; DEC is the HIT POSITION (byte offset in HIT sector) -- NOT the hash VALUE.
            ; V0.1.41 bug: was passing A (hash value) as DEC; caused "Unknown error code".
            LD      A, (MODELID)
            CP      4
            JR      Z, FDIRRD_6
            LD      A, 1
            OR      A
            RET
FDIRRD_6
            LD      A, @DIRRD6
            RST     @SVC
            RET
