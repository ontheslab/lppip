; ----------------------------------------------------------------------
; CRC-16 CCITT verify helpers
; Algorithm from original PPIP v1.8 (D. Jewett III, 1985-1988).
; Same polynomial used by MODEM7 and CHEK: x^16 + x^12 + x^5 + x^0 (0x1021).
; Z80 assembly translated from crc.c in the CPPIP C port.
; ----------------------------------------------------------------------

; INITCRC
;   Build the 256-entry CRC lookup table into CRCTBL (512 bytes).
;   Called once at startup when /V or /C is in use.
;   Inner XOR with 10H/21H implements the 0x1021 polynomial split across H and L.
INITCRC
            LD      HL, CRCTBL
            LD      C, 0            ; C = table index 0..255
CRC_GLOOP
            EX      DE, HL          ; DE = write pointer into CRCTBL
            LD      HL, 0           ; HL = CRC accumulator for this entry
            LD      A, C
            PUSH    BC
            LD      B, 8            ; 8 bits per entry
            XOR     H
            LD      H, A
CRC_LLOOP
            ADD     HL, HL          ; shift CRC left
            JR      NC, CRC_LSKIP
            LD      A, 10H          ; high byte of 0x1021
            XOR     H
            LD      H, A
            LD      A, 21H          ; low byte of 0x1021
            XOR     L
            LD      L, A
CRC_LSKIP
            DEC     B
            JR      NZ, CRC_LLOOP
            POP     BC
            EX      DE, HL          ; HL = write pointer, DE = computed CRC
            LD      (HL), D         ; store high byte
            INC     H               ; CRCTBL is 256-byte-page aligned: high page = high bytes
            LD      (HL), E         ; store low byte in second page
            DEC     H
            INC     HL
            INC     C
            JR      NZ, CRC_GLOOP   ; loop until C wraps 255->0
            RET

; CRC_RESET -- zero the running CRC (CRCVAL)
CRC_RESET
            LD      HL, 0
            LD      (CRCVAL), HL
            RET

; CRC_SAVE_SOURCE -- snapshot running CRC into CRCVAL2 for later comparison
CRC_SAVE_SOURCE
            LD      HL, (CRCVAL)
            LD      (CRCVAL2), HL
            RET

; CRC_UPDATE_A
;   Feed one byte (A) through the CRC lookup table and update CRCVAL.
;   Mirrors crc_update() in crc.c.
;   Input: A = byte to process. Preserves all registers.
CRC_UPDATE_A
            PUSH    BC
            PUSH    HL
            LD      HL, (CRCVAL)
            EX      DE, HL          ; DE = current CRC
            LD      B, 0
            XOR     D               ; index = (CRC >> 8) XOR byte
            LD      C, A
            LD      HL, CRCTBL
            ADD     HL, BC          ; HL -> high-byte page of table entry
            LD      A, (HL)
            XOR     E               ; new CRC high = table[idx].high XOR CRC.low
            LD      D, A
            INC     H               ; step to low-byte page
            LD      E, (HL)         ; new CRC low = table[idx].low
            EX      DE, HL
            LD      (CRCVAL), HL
            POP     HL
            POP     BC
            RET

; CRC_UPDATE_IOBUF_BC
;   Update CRC for BC bytes starting at IOBUF.
;   Input: BC = byte count (0 = nothing to do)
CRC_UPDATE_IOBUF_BC
            LD      HL, IOBUF
CRC_BUF_LOOP
            LD      A, B
            OR      C
            RET     Z
            LD      A, (HL)
            CALL    CRC_UPDATE_A
            INC     HL
            DEC     BC
            JR      CRC_BUF_LOOP

; SRC_REC_BYTES
;   Return the byte count for the current source record in BC.
;   Calls SRC_REC_COUNT (lppip_frehd.inc): returns 0 for last record (use EOF offset)
;   or non-zero for a full record. BC=0100H means 256 bytes (full record).
SRC_REC_BYTES
            CALL    SRC_REC_COUNT
            LD      C, A
            LD      B, 0
            OR      A
            RET     NZ              ; non-zero = full 128-byte record
            INC     B               ; A=0 (last record): B=1, C=0 -> BC=256 (full read)
            RET

; DST_INIT_RECS / DST_DEC_RECS / DST_REC_COUNT / DST_REC_BYTES
;   Mirror of SRC_INIT_RECS group (lppip_frehd.inc), used during the verify
;   pass to walk the destination file with the same record-count logic.
DST_INIT_RECS
            LD      HL, (DST_DCB + DCB_ERN)
            LD      (DST_REMAIN), HL
            RET

DST_DEC_RECS
            LD      HL, (DST_REMAIN)
            LD      A, H
            OR      L
            RET     Z               ; already at zero
            DEC     HL
            LD      (DST_REMAIN), HL
            RET

DST_REC_COUNT
            LD      HL, (DST_REMAIN)
            LD      A, H
            OR      L
            JR      Z, DST_REC_LAST ; remaining = 0, return EOF byte offset
            XOR     A               ; remaining > 0, return 0 = full record
            RET
DST_REC_LAST
            LD      A, (DST_DCB + DCB_EOF)
            RET

DST_REC_BYTES
            CALL    DST_REC_COUNT
            LD      C, A
            LD      B, 0
            OR      A
            RET     NZ
            INC     B               ; A=0: BC=256 (full record)
            RET

; VERIFY_REPORT_OK -- CRC matched; return Z so caller can continue to COPY_SUCCESS
VERIFY_REPORT_OK
            XOR     A
            RET

; VERIFY_COMPARE
;   Compare CRCVAL (destination) against CRCVAL2 (saved source).
;   Z path -> VERIFY_REPORT_OK -> COPY_SUCCESS prints ' OK' and optional CRC.
;   NZ path -> print CRC FAILED and abort.
VERIFY_COMPARE
            LD      HL, (CRCVAL2)   ; source CRC
            LD      DE, (CRCVAL)    ; destination CRC
            OR      A
            SBC     HL, DE
            JR      Z, VERIFY_REPORT_OK
            LD      HL, MSG_CRCFAIL
            CALL    DSPLY               ; cursor mid-line after PSTR_NC(MSG_VERIFY)
            CALL    FABORT

; VERIFY_DOS_DEST
;   Re-open the LS-DOS destination and compute its CRC for comparison.
;   Called after COPY_CLOSE in COPY_LOCAL. Uses DST_REMAIN counter group above.
VERIFY_DOS_DEST
            LD      A, (OPT_VERIFY)
            OR      A
            RET     Z               ; /V not set - skip
            LD      HL, MSG_VERIFY
            CALL    PSTR_NC             ; ' - Verifying' with no CR, stays on line
            CALL    CRC_RESET

            LD      HL, DST_DCB
            LD      BC, DCB_SIZE
            CALL    CLEAR_MEM           ; fresh DCB for verify open

            LD      HL, (DSTPTR)
            LD      DE, DST_DCB
            CALL    FSPEC
            JR      NZ, VERIFY_DOS_SPECERR

            LD      HL, IOBUF
            LD      B, 0
            LD      DE, DST_DCB
            CALL    FOPEN
            JR      Z, VERIFY_DOS_OPENOK
            CP      42                  ; LRL mismatch: file is open, treat as OK
            JR      Z, VERIFY_DOS_OPENOK
VERIFY_DOS_OPENERR
            LD      C, A
            LD      HL, MSG_ERR_VDST
            CALL    DSPLY_NL
            CALL    FERROR
            CALL    FABORT
VERIFY_DOS_SPECERR
            LD      HL, MSG_ERR_VDST
            CALL    DSPLY_NL
            LD      HL, MSG_BADSPEC
            CALL    DSPLY
            CALL    FABORT
VERIFY_DOS_OPENOK
            CALL    DST_INIT_RECS

VERIFY_DOS_LOOP
            LD      DE, DST_DCB
            CALL    FREAD
            JR      Z, VERIFY_DOS_HAVE
            CP      FERR_EOF
            JR      Z, VERIFY_DOS_DONE
            CP      FERR_RNG
            JR      Z, VERIFY_DOS_DONE
            JR      VERIFY_DOS_OPENERR
VERIFY_DOS_HAVE
            CALL    DST_DEC_RECS
            CALL    DST_REC_BYTES
            CALL    CRC_UPDATE_IOBUF_BC
            JR      VERIFY_DOS_LOOP
VERIFY_DOS_DONE
            LD      DE, DST_DCB
            CALL    FCLOSE
            JP      NZ, ERR_CLOSE
            JP      VERIFY_COMPARE

; VERIFY_SD_DEST
;   Re-open the SD destination and compute its CRC for comparison.
;   SD_PATHPTR must already point to the destination path.
VERIFY_SD_DEST
            LD      A, (OPT_VERIFY)
            OR      A
            RET     Z               ; /V not set - skip
            LD      HL, MSG_VERIFY
            CALL    PSTR_NC             ; ' - Verifying' with no CR, stays on line
            CALL    CRC_RESET

            LD      HL, (SD_PATHPTR)
            CALL    SD_OPEN_RD
            JR      Z, VERIFY_SD_OPENOK
            LD      HL, MSG_ERR_VSD
            CALL    DSPLY_NL
            LD      HL, (SD_PATHPTR)
            CALL    PRINT_SD_PATH
            CALL    SHOW_SD_DIAG
            CALL    FABORT
VERIFY_SD_OPENOK
VERIFY_SD_LOOP
            CALL    SD_READ_COUNT
            JR      NZ, VERIFY_SD_FAIL
            LD      A, B
            OR      C
            JR      Z, VERIFY_SD_DONE   ; BC=0 = EOF
            PUSH    BC
            CALL    CRC_UPDATE_IOBUF_BC
            POP     BC
            JR      VERIFY_SD_LOOP
VERIFY_SD_DONE
            CALL    SD_CLOSE
            JR      NZ, VERIFY_SD_FAIL
            JP      VERIFY_COMPARE
VERIFY_SD_FAIL
            CALL    SHOW_SD_DIAG
            CALL    SD_CLOSE
            CALL    FABORT
