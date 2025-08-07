        .ORIG x0100       ; Start of TRAP vector routines

; === TRAP x20 - GETC ===
TRAP_GETC
        LDI R0, KBSR      ; Wait for keypress
WAITK   BRzp WAITK
        LDI R0, KBDR
        RET

; === TRAP x21 - OUT ===
TRAP_OUT
        STI R0, TEMP_OUT  ; Save R0
        LDI R0, DSR
WAITD   BRzp WAITD
        LDR R0, TEMP_OUT, #0
        STI R0, DDR
        LDR R0, TEMP_OUT, #0  ; Restore R0
        RET

; === TRAP x22 - PUTS ===
TRAP_PUTS
        STI R0, SAVE_R0
        STI R1, SAVE_R1
        AND R1, R1, #0
        ADD R1, R0, #0       ; R1 = pointer to string

LOOP_PUTS
        LDR R0, R1, #0       ; Load char
        BRz DONE_PUTS
        JSR TRAP_OUT
        ADD R1, R1, #1
        BR LOOP_PUTS

DONE_PUTS
        LDR R0, SAVE_R0, #0
        LDR R1, SAVE_R1, #0
        RET

; === TRAP x23 - IN ===
TRAP_IN
        LEA R0, PROMPT
        JSR TRAP_PUTS
        JSR TRAP_GETC
        JSR TRAP_OUT
        RET

; === TRAP x24 - PUTSP ===
TRAP_PUTSP
        STI R0, SAVE_R0
        STI R1, SAVE_R1
        AND R1, R1, #0
        ADD R1, R0, #0       ; R1 = pointer to string

LOOP_PUTSP
        LDR R0, R1, #0
        BRz DONE_PUTSP

        ; Low byte
        AND R2, R0, #0xFF
        BRz SKIP_LOW
        ADD R0, R2, #0
        JSR TRAP_OUT
SKIP_LOW
        ; High byte
        LDR R0, R1, #0
        SHR R0, R0, #8
        BRz SKIP_HIGH
        JSR TRAP_OUT
SKIP_HIGH

        ADD R1, R1, #1
        BR LOOP_PUTSP

DONE_PUTSP
        LDR R0, SAVE_R0, #0
        LDR R1, SAVE_R1, #0
        RET

; === TRAP x25 - HALT ===
TRAP_HALT
        LEA R0, HALT_MSG
        JSR TRAP_PUTS
        HALT

; === Data Section ===
KBSR    .FILL xFE00
KBDR    .FILL xFE02
DSR     .FILL xFE04
DDR     .FILL xFE06

TEMP_OUT .BLKW 1
SAVE_R0  .BLKW 1
SAVE_R1  .BLKW 1

PROMPT   .STRINGZ "Input a character: "
HALT_MSG .STRINGZ "HALT called.\n"

        .END

        .ORIG x0000
        .FILL x0100   ; x20 (GETC)
        .FILL x0103   ; x21 (OUT)
        .FILL x0106   ; x22 (PUTS)
        .FILL x010C   ; x23
        .FILL x0110   ; x24 (IN)
        .FILL x0115   ; x25 (HALT)

        .ORIG x3000

        ; Print a character using OUT
        LD R0, CHAR_A      ; Load character 'A' into R0
        TRAP x21           ; OUT

        ; Print a string using PUTS
        LEA R0, HELLO_MSG  ; Load address of HELLO_MSG into R0
        TRAP x22           ; PUTS

        ; Read a character from keyboard (no echo)
        TRAP x20           ; GETC

        ; Read a character from keyboard (with echo)
        TRAP x23           ; IN

        ; Halt the program
        TRAP x25           ; HALT

CHAR_A  .FILL x0041        ; ASCII for 'A'

HELLO_MSG .STRINGZ " Hello from LC3 TRAP test!\n"

        .END
