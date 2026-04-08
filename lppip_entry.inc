; ----------------------------------------------------------------------
; Entry point and top-level dispatch
; ----------------------------------------------------------------------

LPPIP
            LD      (CMDPTR), HL    ; save command line pointer FIRST - HL on entry = params
            CALL    WHATVER         ; identify model (A=1/3/4)
            LD      (MODELID), A
            CP      4
            JR      NZ, LPPIP_RUN  ; not Model 4 - use Model I/III table as-is
            ; Copy Model 4 SVC table over the active Model I/III table at runtime.
            ; Both tables are the same size (ENDJ - STARTJ bytes) by construction --
            ; each entry's byte counts in the source are there to verify this invariant.
            LD      HL, STARTJ6
            LD      DE, STARTJ
            LD      BC, ENDJ - STARTJ
            LDIR                   ; overwrite active table with Model 4 SVC table

LPPIP_RUN
            CALL    SETUP           ; enable external I/O if needed
            LD      HL, MSG_BANNER
            CALL    PSTR_NC         ; print banner without trailing CR
            CALL    SD_PRESENT
            LD      HL, MSG_SD_FOUND
            JR      Z, LPPIP_SDOK
            LD      HL, MSG_SD_NOTFOUND
LPPIP_SDOK
            CALL    DSPLY           ; appends FreHD status + CR to banner line

            ; zmac's .cmd output does not guarantee DEFS space is zero-filled at load time.
            ; Unlike z88dk COM output, zmac CMD files may contain uninitialised data in BSS.
            ; Explicit clear avoids false flag/counter reads on first use.
            LD      HL, SRC_NAME
            LD      BC, WORK_SIZE
            CALL    CLEAR_MEM

            ; Clear DST_NAME so "no dest given" check works on uninitialised memory
            LD      A, CR
            LD      (DST_NAME), A
            XOR     A
            LD      (OPT_VERIFY), A
            LD      (OPT_REPORT), A
            LD      (OPT_EMEND), A
            LD      (OPT_WIPE), A

            ; Restore saved command line pointer (HL at entry = params, saved before WHATVER)
            LD      HL, (CMDPTR)
            CALL    SKIPSP

            ; Empty command line - show usage
            LD      A, (HL)
            CP      CR
            JP      Z, DO_USAGE

            ; Check for /H as sole argument (LPPIP /H)
            CP      '/'
            JR      NZ, LPPIP_PARSE
            PUSH    HL
            INC     HL
            LD      A, (HL)
            POP     HL
            OR      20H             ; fold to lowercase
            CP      'h'
            JP      Z, DO_HELP
LPPIP_PARSE

            ; Parse both filespecs from command line before touching any files
            LD      (SRCPTR), HL
            LD      DE, SRC_NAME
            CALL    COPYTOKEN       ; HL advances to delimiter
            CALL    SKIPSP
            LD      A, (HL)
            CP      CR
            JP      Z, DO_USAGE     ; no dest given
            LD      (DSTPTR), HL
            LD      DE, DST_NAME
            CALL    COPYTOKEN
            CALL    PARSE_OPTIONS

            LD      A, (OPT_VERIFY)
            OR      A
            CALL    NZ, INITCRC

            LD      HL, SRC_NAME
            CALL    IS_SD_SPEC
            LD      (SRC_IS_SD), A
            LD      HL, DST_NAME
            CALL    IS_SD_SPEC
            LD      (DST_IS_SD), A

            LD      A, (SRC_IS_SD)
            OR      A
            JR      Z, CHECK_DST_SD
            LD      A, (DST_IS_SD)
            OR      A
            JP      NZ, ERR_SD2SD
            LD      HL, SRC_NAME + 3
            CALL    HAS_WILD_SPEC
            OR      A
            JP      NZ, COPY_SD_WILD_TO_DOS
            CALL    EXPAND_SD_BARE_DST  ; ':digit' -> 'NAME/EXT:digit' for SD src
            CALL    SHOW_COPY_NAMES
            CALL    DEST_EXISTS_DOS
            JP      Z, COPY_SD_TO_DOS
            JP      FEXIT

CHECK_DST_SD
            LD      HL, SRC_NAME
            CALL    HAS_WILD_SPEC
            OR      A
            JR      Z, CHECK_DST_SD_ONLY
            LD      A, (DST_IS_SD)
            OR      A
            JP      NZ, COPY_DOS_WILD_TO_SD
            JP      COPY_DOS_WILD_TO_DOS
CHECK_DST_SD_ONLY
            LD      A, (DST_IS_SD)
            OR      A
            JR      Z, DO_LOCAL_COPY
            CALL    SHOW_COPY_NAMES
            JP      COPY_DOS_TO_SD
DO_LOCAL_COPY
            CALL    EXPAND_BARE_DST
            CALL    SHOW_COPY_NAMES
            CALL    DEST_EXISTS_DOS
            JP      Z, COPY_LOCAL
            JP      FEXIT           ; user said no / R/O without /W
