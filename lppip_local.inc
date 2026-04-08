COPY_LOCAL
            ; Open source first and prove the first read works before creating dest.
            ; Pattern from export2.z80 / import2_ldos.z80 references.
            ; LS-DOS @OPEN: DE=DCB, HL=buffer, B=LRL (0=256)
            LD      HL, (SRCPTR)
            LD      DE, SRC_DCB
            CALL    FSPEC
            JP      NZ, ERR_SPEC

            LD      HL, IOBUF
            LD      B, 0
            LD      DE, SRC_DCB
            CALL    FOPEN
            JR      Z, FOPEN_OK
            CP      42              ; lrlerr: LRL mismatch, file still open - ignore
            JP      NZ, ERR_OPEN_SRC
FOPEN_OK
            LD      A, (OPT_VERIFY)
            OR      A
            CALL    NZ, CRC_RESET
            CALL    SRC_INIT_RECS


            ; First read with only the source open. If this works, any later
            ; failure is in the dual-open copy path rather than the read path.
            LD      DE, SRC_DCB
            CALL    FREAD
            JR      Z, FIRST_READ_OK
            CP      FERR_EOF
            JR      Z, EMPTY_SOURCE
            CP      FERR_RNG
            JR      Z, EMPTY_SOURCE
            LD      C, A
            CALL    FERROR
            JP      COPY_ABORT_SRC

FIRST_READ_OK
            CALL    SRC_DEC_RECS
            LD      A, (OPT_VERIFY)
            OR      A
            JR      Z, FIRST_READ_NOV
            CALL    SRC_REC_BYTES
            CALL    CRC_UPDATE_IOBUF_BC
FIRST_READ_NOV

            ; Create/init destination only after the source read succeeds.
            LD      HL, (DSTPTR)
            LD      DE, DST_DCB
            CALL    FSPEC
            JP      NZ, ERR_SPEC_CLOSE_SRC

            LD      HL, DST_BUF
            LD      B, 0
            LD      DE, DST_DCB
            CALL    FINIT
            JR      Z, FINIT_OK
            CP      42
            JP      NZ, ERR_OPEN_DST
FINIT_OK
            JP      COPY_WRITE

EMPTY_SOURCE
            ; Empty source: still create the destination so empty copies work.
            LD      HL, (DSTPTR)
            LD      DE, DST_DCB
            CALL    FSPEC
            JP      NZ, ERR_SPEC_CLOSE_SRC

            LD      HL, DST_BUF
            LD      B, 0
            LD      DE, DST_DCB
            CALL    FINIT
            JR      Z, COPY_EOF
            CP      42
            JP      NZ, ERR_OPEN_DST
            JP      COPY_EOF

; Copy loop - read remaining records from src, write to dst
COPY_LOOP
            LD      DE, SRC_DCB
            CALL    FREAD
            JR      Z, COPY_READ_OK         ; Z = success, got a record
            CP      FERR_EOF                ; NZ - is it EOF?
            JR      Z, COPY_EOF             ; yes - normal end of file
            CP      FERR_RNG                ; NRN > ERN is also normal EOF here
            JR      Z, COPY_EOF
            LD      C, A                    ; no - read error (save code for FERROR)
            CALL    FERROR
            JP      COPY_ABORT

COPY_READ_OK
            CALL    SRC_DEC_RECS
            LD      A, (OPT_VERIFY)
            OR      A
            JR      Z, COPY_WRITE
            CALL    SRC_REC_BYTES
            CALL    CRC_UPDATE_IOBUF_BC

COPY_WRITE
            ; Re-entry point from FINIT_OK: the first record is already in IOBUF
            ; from the pre-create read above. Write it before looping for the rest.
            LD      HL, IOBUF
            LD      (DST_DCB + DCB_BUF), HL ; point dst DCB buffer at IOBUF
            LD      DE, DST_DCB
            CALL    FWRITE
            JR      Z, COPY_LOOP
            LD      C, A                    ; write error
            CALL    FERROR
            JP      COPY_ABORT

COPY_EOF
            ; All records copied - set EOF offset and ERN in dst DCB
            LD      A, (SRC_DCB + DCB_EOF)  ; copy EOF byte offset from src
            LD      (DST_DCB + DCB_EOF), A
            LD      HL, (DST_DCB + DCB_NRN) ; SETERN: ERN = NRN (Model 4 / LS-DOS)
            LD      (DST_DCB + DCB_ERN), HL
            LD      A, (OPT_VERIFY)
            OR      A
            CALL    NZ, CRC_SAVE_SOURCE

COPY_CLOSE
            LD      DE, SRC_DCB
            CALL    FCLOSE
            LD      DE, DST_DCB
            CALL    FCLOSE
            JP      NZ, ERR_CLOSE
            CALL    VERIFY_DOS_DEST
            JP      COPY_SUCCESS

COPY_ABORT
            ; Error already displayed - close src (ignore errors) and abort
            LD      DE, SRC_DCB
            CALL    FCLOSE
            LD      DE, DST_DCB
            CALL    FCLOSE
            CALL    FABORT

COPY_ABORT_SRC
            LD      DE, SRC_DCB
            CALL    FCLOSE
            CALL    FABORT

COPY_ABORT_SD
            CALL    SD_CLOSE
            LD      DE, DST_DCB
            CALL    FCLOSE
            CALL    FABORT

COPY_ABORT_SD_SRC
            CALL    SD_CLOSE
            LD      DE, SRC_DCB
            CALL    FCLOSE
            CALL    FABORT

EXPAND_BARE_DST
            ; V0.1.45: @FSPEC (SVC 78) cannot parse ':digit' alone - colon is not a
            ; valid name-start character so @FSPEC returns NZ immediately with "BAD FILESPEC".
            ; Fix: detect ':digit' in DST_NAME, build a full spec from SRC_NAME + ':digit'.
            LD      A, (DST_NAME)
            CP      ':'
            RET     NZ
            LD      A, (DST_NAME + 1)
            CP      '0'
            RET     C
            CP      '8'
            RET     NC
            LD      B, A            ; save drive digit

            LD      HL, SRC_NAME
            LD      DE, DST_NAME
EBD_COPY
            LD      A, (HL)
            CP      CR
            JR      Z, EBD_DRV
            CP      ':'
            JR      Z, EBD_DRV
            LD      (DE), A
            INC     HL
            INC     DE
            JR      EBD_COPY
EBD_DRV
            LD      A, ':'
            LD      (DE), A
            INC     DE
            LD      A, B
            LD      (DE), A
            INC     DE
            LD      A, CR
            LD      (DE), A
            LD      HL, DST_NAME
            LD      (DSTPTR), HL
            RET
