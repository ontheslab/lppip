; ----------------------------------------------------------------------
; Display helpers and formatted output
; Separated from lppip_helpers.inc so other tools can include this
; module independently without pulling in the full utility helper set.
; ----------------------------------------------------------------------

; Print a CR-terminated string WITHOUT outputting the CR itself.
; On return cursor is positioned immediately after the last character.
; Use this when the next print continues on the same line.
PSTR_NC
            LD      A, (HL)
            CP      CR
            RET     Z
            CALL    DSP
            INC     HL
            JR      PSTR_NC

; Output a CR to start a fresh line, then display a CR-terminated string.
; Use in error handlers that may be called while the cursor is mid-line
; (i.e. after SHOW_COPY_NAMES which leaves no trailing CR).
DSPLY_NL
            PUSH    HL
            LD      A, CR
            CALL    DSP
            POP     HL
            JP      DSPLY

; Print  "SRC -> DEST"  with no trailing CR.
; COPY_SUCCESS prints " OK\r" to complete the line.
; Error handlers use DSPLY_NL to start a fresh line before their message.
SHOW_COPY_NAMES
            LD      HL, SRC_NAME
            CALL    PSTR_NC
            LD      HL, MSG_ARROW
            CALL    PSTR_NC
            LD      HL, DST_NAME
            CALL    PSTR_NC
            RET

; Print ' OK' on the current line (after src->dest and optional verify text),
; optionally append '  CRC: xxxx' if /C was given, then end the line.
; In single-file mode (BATCH_MODE=0) falls through to FEXIT and exits the program.
; In wildcard mode (BATCH_MODE=1) returns to the caller so the loop continues.
COPY_SUCCESS
            LD      HL, MSG_OK
            CALL    PSTR_NC             ; ' OK' without CR, cursor stays on line
            LD      A, (OPT_REPORT)
            OR      A
            JR      Z, CS_EOL
            LD      HL, MSG_CRC         ; '  CRC: ' (NUL-terminated, see lppip_data.inc)
            CALL    PSTRZ
            LD      HL, (CRCVAL2)       ; source CRC saved before verify pass
            CALL    PRHEX16
CS_EOL
            LD      A, CR
            CALL    DSP
            LD      A, (BATCH_MODE)
            OR      A
            RET     NZ                  ; wildcard loop: return to caller
            JP      FEXIT               ; single-file: exit program

; Print a NUL-terminated string (used for CRC value prefix etc.)
PSTRZ
            LD      A, (HL)
            OR      A
            RET     Z
            CALL    DSP
            INC     HL
            JR      PSTRZ

; PRHEX8 -- print A as two uppercase hex digits.
; Four RRCAs rotate the high nibble into the low position for the first PRNIBBLE call.
PRHEX8
            PUSH    AF
            RRCA
            RRCA
            RRCA
            RRCA
            CALL    PRNIBBLE
            POP     AF
            CALL    PRNIBBLE
            RET

; PRNIBBLE -- print the low nibble of A as one hex digit (0-9, A-F).
; ADD A,'0' maps 0-9 to '0'-'9'. For A-F the result lands in ':'-'@' (0x3A-0x40),
; which are not hex digits. ADD A,7 jumps over that gap to 'A'-'F' (0x41-0x46).
PRNIBBLE
            AND     0FH
            ADD     A, '0'
            CP      '9' + 1
            JR      C, PRNIBBLE_OUT
            ADD     A, 7            ; bridge the ':'-'@' gap to reach 'A'-'F'
PRNIBBLE_OUT
            CALL    DSP
            RET

; PRHEX16 -- print HL as four uppercase hex digits (high byte first).
PRHEX16
            PUSH    HL
            LD      A, H
            CALL    PRHEX8
            LD      A, L
            CALL    PRHEX8
            POP     HL
            RET
