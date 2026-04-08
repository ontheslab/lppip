; ----------------------------------------------------------------------
; Destination file-exists check and overwrite handling.
; ----------------------------------------------------------------------

; DEST_EXISTS_DOS
; Check whether the destination (DSTPTR) already exists on an LS-DOS drive.
; Skips the check when DST_IS_SD is set (SD destinations always overwrite).
;
; On return: Z = proceed with copy, NZ = skip this file (user said no,
;            or file is R/O and /W not set).
;
; Registers: all preserved except AF.
; SHOW_COPY_NAMES must already have been called so the cursor is mid-line.
;
DEST_EXISTS_DOS
            LD      A, (DST_IS_SD)
            OR      A
            JR      NZ, DED_PROCEED     ; SD dest: no LS-DOS exist check

            ; Parse dest spec into DST_DCB for the probe open.
            PUSH    HL
            PUSH    DE
            PUSH    BC
            LD      HL, (DSTPTR)
            LD      DE, DST_DCB
            CALL    FSPEC
            JR      NZ, DED_NOCLEAN     ; bad spec -> proceed (FINIT will also fail)

            ; Try to open the destination file as a probe.
            LD      HL, DST_BUF
            LD      B, 0
            LD      DE, DST_DCB
            CALL    FOPEN
            JR      Z, DED_EXISTS
            CP      42                  ; LRL mismatch: file is open, treat as exists
            JR      Z, DED_EXISTS
            ; Open failed = file does not exist: proceed normally.
            JR      DED_NOCLEAN

DED_EXISTS
            ; File is open. Read the protection bit from the DCB.
            LD      A, (DST_DCB + DCB_FLAGS)
            AND     02H                 ; bit 1 = R/O (write-protected)
            PUSH    AF                  ; save R/O flag

            ; Close the probe.
            LD      DE, DST_DCB
            CALL    FCLOSE

            POP     AF                  ; restore R/O flag
            JR      Z, DED_RW           ; Z = not R/O

            ; --- R/O file ---
            LD      A, (OPT_WIPE)
            OR      A
            JR      NZ, DED_DELETE      ; /W: delete it, proceed
            ; No /W: skip with message.
            LD      HL, MSG_SKIP_RO
            CALL    DSPLY_NL
            POP     BC
            POP     DE
            POP     HL
            OR      1                   ; NZ = skip
            RET

DED_RW
            ; --- R/W file ---
            LD      A, (OPT_EMEND)
            OR      A
            JR      NZ, DED_NOCLEAN     ; /E: FINIT will truncate/recreate, proceed
            LD      A, (OPT_WIPE)
            OR      A
            JR      NZ, DED_NOCLEAN     ; /W: same

            ; Ask the user.
            LD      HL, MSG_EXISTS
            CALL    PSTR_NC             ; 'EXISTS: OVERWRITE? (Y/N) ' stays on line
            CALL    ASK_KEY             ; returns A = uppercase key
            CP      'Y'
            JR      Z, DED_YES
            ; Not Y: skip.
            LD      HL, MSG_SKIP
            CALL    DSPLY_NL
            POP     BC
            POP     DE
            POP     HL
            OR      1                   ; NZ = skip
            RET

DED_YES
DED_NOCLEAN
            POP     BC
            POP     DE
            POP     HL
DED_PROCEED
            XOR     A                   ; Z = proceed
            RET

DED_DELETE
            ; Delete the file. Re-parse the filespec into a clean DCB before calling FREMOV.
            ; The prior FOPEN/FCLOSE left internal state in DST_DCB; @SCRATCH needs it fresh.
            POP     BC
            POP     DE
            POP     HL
            PUSH    HL
            PUSH    DE
            PUSH    BC
            LD      HL, (DSTPTR)
            LD      DE, DST_DCB
            CALL    FSPEC
            LD      DE, DST_DCB
            CALL    FREMOV
            POP     BC
            POP     DE
            POP     HL
            XOR     A                   ; Z = proceed
            RET

; DEST_EXISTS_SD
; Check whether the SD destination already exists on the FreHD.
; SD_PATHPTR must be set to the path before calling (set by BUILD_SD_DST_PATH).
; SHOW_COPY_NAMES must already have been called so the cursor is mid-line.
;
; On return: Z = proceed with copy, NZ = skip this file.
; /E or /W: proceed without prompting (overwrite silently).
;
DEST_EXISTS_SD
            CALL    SD_OPEN_RD          ; probe: can we open it for read?
            JR      NZ, DESD_PROCEED    ; not found or I/O error: proceed

            ; File exists. Close the probe.
            CALL    SD_CLOSE            ; ignore any close error

            ; /E or /W: overwrite silently.
            LD      A, (OPT_EMEND)
            OR      A
            JR      NZ, DESD_PROCEED
            LD      A, (OPT_WIPE)
            OR      A
            JR      NZ, DESD_PROCEED

            ; Ask the user.
            LD      HL, MSG_EXISTS
            CALL    PSTR_NC
            CALL    ASK_KEY
            CP      'Y'
            JR      Z, DESD_PROCEED
            LD      HL, MSG_SKIP
            CALL    DSPLY_NL
            OR      1                   ; NZ = skip
            RET

DESD_PROCEED
            XOR     A                   ; Z = proceed
            RET

; ----------------------------------------------------------------------
; ASK_KEY
; Read a single keypress and return it as uppercase in A.
; On Model 4: polls @@KEY (SVC 1) until a key is pressed.
; On Model I/III: calls @KEYIN ($0040) for line input; returns first char.
; Note: $0040 address for Model I/III needs hardware verification.
; ----------------------------------------------------------------------

@KEYIN_M13  EQU     0040H   ; line input entry, Model I/III (unverified)
@KEY6       EQU     1       ; @@KEY SVC number (Model 4)

ASK_KEY
            LD      A, (MODELID)
            CP      4
            JR      NZ, ASK_KEY_M13

ASK_KEY_M4
            ; Non-blocking poll until a key arrives.
            LD      A, @KEY6
            RST     @SVC            ; A = key code, 0 = no key
            OR      A
            JR      Z, ASK_KEY_M4
            CALL    UPCASE_A
            RET

ASK_KEY_M13
            ; Line input: user types a char then ENTER.
            LD      HL, KEYIN_BUF
            LD      B, 3            ; max 3 chars (Y/N + CR + safety)
            CALL    @KEYIN_M13
            LD      A, (KEYIN_BUF)
            CALL    UPCASE_A
            RET
