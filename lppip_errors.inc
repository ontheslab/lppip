; ----------------------------------------------------------------------
; Error handlers
; All errors that fire after SHOW_COPY_NAMES use DSPLY_NL so they start
; on a fresh line (SHOW_COPY_NAMES leaves no trailing CR on the line).
; DO_USAGE always fires before SHOW_COPY_NAMES so it uses plain DSPLY.
; ----------------------------------------------------------------------

ERR_OPEN_SRC
            ; V0.1.66: @ERROR6 on a damaged-sector volume triggers a LS-DOS fatal crash
            ; rather than returning cleanly. Guard: only call FERROR in single-file mode.
            LD      C, A
            LD      HL, MSG_ERR_SRC
            CALL    DSPLY_NL
            LD      A, (BATCH_MODE)
            OR      A
            JR      NZ, ERR_SRC_BATCH
            CALL    FERROR          ; safe only in single-file mode (see note above)
            CALL    FABORT
ERR_SRC_BATCH
            LD      HL, MSG_SKIP
            CALL    DSPLY
            LD      DE, SRC_DCB     ; V0.1.67: failed @FOPEN leaves SRC_DCB dirty; FCLOSE clears it
            CALL    FCLOSE          ; ignore return - may fail if @FOPEN never opened it
            POP     HL              ; discard caller's return addr (one level: CDW or CDWSD loop)
            ; V0.1.68: must dispatch to the correct loop's NEXT label.
            ; Jumping to CDW_NEXT when inside CDWSD_LOOP caused BUILD_DOS_DST_LOCAL to
            ; reject the SD destination and silently skip every remaining file (NO FILE(S) FOUND).
            LD      A, (DST_IS_SD)  ; which loop are we in?
            OR      A
            JP      NZ, CDWSD_NEXT  ; DOS->SD wildcard: resume CDWSD_LOOP
            JP      CDW_NEXT        ; DOS->DOS wildcard: resume CDW_LOOP

ERR_OPEN_DST
            LD      C, A
            LD      HL, MSG_ERR_DST
            CALL    DSPLY_NL
            CALL    FERROR
            LD      DE, SRC_DCB     ; close src before aborting
            CALL    FCLOSE
            CALL    FABORT

ERR_OPEN_DST_CLOSE_SD
            LD      C, A
            LD      HL, MSG_ERR_DST
            CALL    DSPLY_NL
            CALL    FERROR
            CALL    SD_CLOSE
            CALL    FABORT

ERR_CLOSE
            LD      C, A
            LD      HL, MSG_ERR_CLOSE
            CALL    DSPLY_NL
            CALL    FERROR
            CALL    FABORT

ERR_NOSD
            LD      HL, MSG_ERR_NOSD
            CALL    DSPLY_NL
            CALL    FABORT

ERR_SD2SD
            LD      HL, MSG_ERR_SD2SD
            CALL    DSPLY_NL
            CALL    FABORT

ERR_SDNAME
            LD      HL, MSG_ERR_SDNAME
            CALL    DSPLY_NL
            CALL    FABORT

ERR_WILDDST
            LD      HL, MSG_ERR_WILDDST
            CALL    DSPLY_NL
            CALL    FABORT

ERR_OPEN_SDSRC
            LD      HL, MSG_ERR_SDSRC
            CALL    DSPLY_NL
            LD      HL, (SD_PATHPTR)
            CALL    PRINT_SD_PATH
            CALL    SHOW_SD_DIAG
            CALL    FABORT

ERR_OPEN_SDDST_CLOSE_SRC
            LD      HL, MSG_ERR_SDDST
            CALL    DSPLY_NL
            LD      HL, (SD_PATHPTR)
            CALL    PRINT_SD_PATH
            CALL    SHOW_SD_DIAG
            LD      DE, SRC_DCB
            CALL    FCLOSE
            CALL    FABORT

ERR_SPEC
            LD      HL, MSG_BADSPEC
            CALL    DSPLY_NL
            CALL    FABORT

ERR_SPEC_CLOSE_SD
            LD      HL, MSG_BADSPEC
            CALL    DSPLY_NL
            CALL    SD_CLOSE
            CALL    FABORT

ERR_SPEC_CLOSE_SRC
            LD      HL, MSG_BADSPEC
            CALL    DSPLY_NL
            LD      DE, SRC_DCB
            CALL    FCLOSE
            CALL    FABORT

ERR_SDIO_CLOSE_SD
            CALL    SHOW_SD_DIAG
            CALL    SD_CLOSE
            CALL    FABORT

ERR_SDIO_CLOSE_DST
            CALL    SHOW_SD_DIAG
            LD      DE, DST_DCB
            CALL    FCLOSE
            CALL    FABORT

ERR_SDIO_CLOSE_SRC
            CALL    SHOW_SD_DIAG
            LD      DE, SRC_DCB
            CALL    FCLOSE
            CALL    FABORT

ERR_SDIO_CLOSE_BOTH
            CALL    SHOW_SD_DIAG
            CALL    SD_CLOSE
            LD      DE, DST_DCB
            CALL    FCLOSE
            CALL    FABORT

ERR_SDIO_CLOSE_SRCSD
            CALL    SHOW_SD_DIAG
            CALL    SD_CLOSE
            LD      DE, SRC_DCB
            CALL    FCLOSE
            CALL    FABORT

DO_HELP
            LD      HL, MSG_USAGE
            CALL    DSPLY
            LD      HL, MSG_HELP_1
            CALL    DSPLY
            LD      HL, MSG_HELP_2
            CALL    DSPLY
            LD      HL, MSG_HELP_3
            CALL    DSPLY
            LD      HL, MSG_HELP_4
            CALL    DSPLY
            LD      HL, MSG_HELP_5
            CALL    DSPLY
            LD      HL, MSG_HELP_6
            CALL    DSPLY
            LD      HL, MSG_HELP_7
            CALL    DSPLY
            LD      HL, MSG_HELP_8
            CALL    DSPLY
            LD      HL, MSG_HELP_9
            CALL    DSPLY
            LD      HL, MSG_HELP_A
            CALL    DSPLY
            LD      HL, MSG_HELP_B
            CALL    DSPLY
            LD      HL, MSG_HELP_C
            CALL    DSPLY
            LD      HL, MSG_HELP_D
            CALL    DSPLY
            LD      HL, MSG_HELP_E
            CALL    DSPLY
            LD      HL, MSG_HELP_F
            CALL    DSPLY
            LD      HL, MSG_HELP_G
            CALL    DSPLY
            LD      HL, MSG_HELP_H
            CALL    DSPLY
            CALL    FEXIT

DO_USAGE
            LD      HL, MSG_USAGE
            CALL    DSPLY           ; before SHOW_COPY_NAMES - cursor already on fresh line
            CALL    FABORT
