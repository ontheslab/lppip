; ----------------------------------------------------------------------
; Generic helpers
; ----------------------------------------------------------------------

; COPYTOKEN
;   Copy one whitespace-delimited token from command line into dest buffer.
;   Input:  HL = source (command line pointer), DE = dest buffer
;   Output: dest is CR-terminated; HL left pointing at the delimiter (SPC or CR)
COPYTOKEN
            LD      A, (HL)
            CP      SPC             ; space = end of token
            JR      Z, CTDONE
            CP      CR              ; CR = end of line
            JR      Z, CTDONE
            LD      (DE), A
            INC     HL
            INC     DE
            JR      COPYTOKEN
CTDONE
            LD      A, CR           ; CR-terminate the dest buffer
            LD      (DE), A
            RET

; COPYCRSTR
;   Copy a CR-terminated string from HL to DE, including the terminating CR.
;   Input:  HL = source, DE = dest
;   Output: DE points one past the copied CR
COPYCRSTR
            LD      A, (HL)
            LD      (DE), A
            INC     HL
            INC     DE
            CP      CR
            JR      NZ, COPYCRSTR
            RET

; SKIPSP
;   Advance HL past any space characters.
;   Input:  HL = pointer into string
;   Output: HL points at first non-space (or CR if end of line)
SKIPSP
            LD      A, (HL)
            CP      SPC
            RET     NZ
            INC     HL
            JR      SKIPSP

; PARSE_OPTIONS
;   Scan the rest of the command line for option switches after both filespecs.
;   Input:  HL = pointer into command line
;   Sets:   OPT_VERIFY, OPT_REPORT, OPT_EMEND, OPT_WIPE (flags in lppip_data.inc)
;   Recognised: /V (verify), /C (CRC report, implies /V), /E (overwrite R/W),
;               /W (overwrite R/O+R/W), /H (help). Unknown option -> DO_USAGE.
;   Supports grouped (/VC) and spaced (/ V /C) forms.
PARSE_OPTIONS
            CALL    SKIPSP
POPT_NEXT
            LD      A, (HL)
            CP      CR
            RET     Z               ; end of line - done
            CP      '/'
            JP      NZ, DO_USAGE    ; non-slash char - bad usage
            INC     HL
POPT_TOKEN
            LD      A, (HL)
            CP      SPC
            JR      Z, POPT_GAP     ; space after slash - skip to next token
            CP      CR
            RET     Z               ; end of line - done
            CALL    UPCASE_A
            CP      'V'
            JR      Z, POPT_SETV
            CP      'C'
            JR      Z, POPT_SETC
            CP      'E'
            JR      Z, POPT_SETE
            CP      'W'
            JR      Z, POPT_SETW
            CP      'H'
            JP      Z, DO_HELP
            JP      DO_USAGE        ; unknown option letter
POPT_SETV
            LD      A, 1
            LD      (OPT_VERIFY), A
            INC     HL
            JR      POPT_TOKEN
POPT_SETC
            LD      A, 1
            LD      (OPT_REPORT), A
            LD      (OPT_VERIFY), A ; /C implies /V
            INC     HL
            JR      POPT_TOKEN
POPT_SETE
            LD      A, 1
            LD      (OPT_EMEND), A
            INC     HL
            JR      POPT_TOKEN
POPT_SETW
            LD      A, 1
            LD      (OPT_WIPE), A
            INC     HL
            JR      POPT_TOKEN
POPT_GAP
            CALL    SKIPSP
            JR      POPT_NEXT

; CLEAR_MEM
;   Fill BC bytes at HL with zero.
;   Input:  HL = start address, BC = byte count
;   Uses 16-bit BC (not DJNZ) to handle counts > 255.
CLEAR_MEM
            LD      A, B
            OR      C
            RET     Z               ; nothing to clear
            XOR     A
CLEAR_LOOP
            LD      (HL), A
            INC     HL
            DEC     BC
            LD      A, B
            OR      C
            JR      NZ, CLEAR_LOOP
            RET

; UPCASE_A
;   Convert A to uppercase if it is a lowercase ASCII letter; leave all other
;   characters (digits, punctuation, already-upper) unchanged.
;   Input/Output: A
UPCASE_A
            CP      'a'
            RET     C               ; below 'a' - not lowercase
            CP      'z' + 1
            RET     NC              ; above 'z' - not lowercase
            SUB     20H             ; flip bit5: lowercase -> uppercase
            RET

; IS_SD_SPEC
;   Check whether the string at HL begins with "SD:" (case-insensitive).
;   Input:  HL = pointer to filespec string (not advanced)
;   Output: A = 1 if SD: prefix present, A = 0 if not
IS_SD_SPEC
            LD      A, (HL)
            CALL    UPCASE_A
            CP      'S'
            JR      NZ, IS_SD_NO
            INC     HL
            LD      A, (HL)
            CALL    UPCASE_A
            CP      'D'
            JR      NZ, IS_SD_NO
            INC     HL
            LD      A, (HL)
            CP      ':'
            JR      NZ, IS_SD_NO
            LD      A, 1
            RET
IS_SD_NO
            XOR     A
            RET

; HAS_WILD_SPEC
;   Scan string at HL for a wildcard character (* or ?) before CR.
;   Input:  HL = pointer to string (not advanced)
;   Output: A = 1 if wildcard found, A = 0 if not
HAS_WILD_SPEC
HWS_LOOP
            LD      A, (HL)
            CP      CR
            JR      Z, HWS_NO
            CP      '*'
            JR      Z, HWS_YES
            CP      '?'
            JR      Z, HWS_YES
            INC     HL
            JR      HWS_LOOP
HWS_YES
            LD      A, 1
            RET
HWS_NO
            XOR     A
            RET

; SHOW_COPY_NAMES, COPY_SUCCESS, PSTRZ, PRHEX8/16 are in lppip_display.inc
