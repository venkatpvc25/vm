; === Trap vector table ===
        .ORIG x0020          ; Start of the TRAP vector table at address x0020
        .FILL x0100          ; TRAP x20 points to handler at x0100 (GETC)
        .FILL x0106          ; TRAP x21 points to handler at x0106 (OUT)
        .FILL x010E          ; TRAP x22 points to handler at x010E (PUTS)
        .FILL x0120          ; TRAP x23 points to handler at x0120 (IN)
        .FILL x0128          ; TRAP x24 points to handler at x0128 (PUTSP)
        .FILL x0138          ; TRAP x25 points to handler at x0138 (HALT)

; === TRAP x20 - GETC ===
        .ORIG x0100          ; Start TRAP_GETC handler at x0100
TRAP_GETC
        LDI R0, KBSR         ; Load keyboard status register content into R0
WAITK   BRzp WAITK           ; Loop until keyboard status bit 15 = 1 (key pressed)
        LDI R0, KBDR         ; Load keyboard data register (actual char) into R0
        RET                  ; Return from trap

; === TRAP x21 - OUT ===
        .ORIG x0106          ; Start TRAP_OUT handler at x0106
TRAP_OUT
        STI R0, TEMP_OUT     ; Store R0 temporarily
        LDI R0, DSR          ; Load display status register
WAITD   BRzp WAITD           ; Wait until DSR ready to send (bit 15 = 0)
        LDR R0, TEMP_OUT, #0 ; Load char back to R0
        STI R0, DDR          ; Store char in display data register (output)
        LDR R0, TEMP_OUT, #0 ; Restore R0 (optional)
        RET                  ; Return from trap

; === TRAP x22 - PUTS ===
        .ORIG x010E          ; Start TRAP_PUTS handler at x010E
TRAP_PUTS
        STI R0, SAVE_R0      ; Save R0
        STI R1, SAVE_R1      ; Save R1
        AND R1, R1, #0       ; Clear R1 (string pointer)
        ADD R1, R1, R0       ; R1 = address of string (original R0)
LOOP_PUTS
        LDR R0, R1, #0       ; Load char at R1
        BRz DONE_PUTS        ; If zero, end string
        JSR TRAP_OUT         ; Output char
        ADD R1, R1, #1       ; Move pointer forward
        BR LOOP_PUTS         ; Loop again
DONE_PUTS
        LDR R0, SAVE_R0, #0  ; Restore R0
        LDR R1, SAVE_R1, #0  ; Restore R1
        RET                  ; Return from trap

; === TRAP x23 - IN ===
        .ORIG x0120          ; Start TRAP_IN handler at x0120
TRAP_IN
        LEA R0, PROMPT       ; Load prompt string address into R0
        JSR TRAP_PUTS        ; Output prompt
        JSR TRAP_GETC        ; Get one char input (R0 updated)
        JSR TRAP_OUT         ; Echo input char
        RET                  ; Return from trap

; === TRAP x24 - PUTSP ===
        .ORIG x0128          ; Start TRAP_PUTSP handler at x0128
TRAP_PUTSP
        STI R0, SAVE_R0      ; Save R0
        STI R1, SAVE_R1      ; Save R1
        AND R1, R1, #0       ; Clear R1 (pointer)
        ADD R1, R1, R0       ; R1 = address of string (word string)
LOOP_PUTSP
        LDR R0, R1, #0       ; Load word from string
        BRz DONE_PUTSP       ; If zero, end string
        AND R2, R0, #255     ; Extract low byte
        BRz SKIP_LOW         ; Skip if zero
        ADD R0, R2, #0       ; Move low byte to R0
        JSR TRAP_OUT         ; Output low byte
SKIP_LOW
        LDR R0, R1, #0       ; Reload word
        SHR R0, R0, #8       ; Shift right 8 bits (high byte)
        BRz SKIP_HIGH        ; Skip if zero
        JSR TRAP_OUT         ; Output high byte
SKIP_HIGH
        ADD R1, R1, #1       ; Next word
        BR LOOP_PUTSP        ; Loop
DONE_PUTSP
        LDR R0, SAVE_R0, #0  ; Restore R0
        LDR R1, SAVE_R1, #0  ; Restore R1
        RET                  ; Return from trap

; === TRAP x25 - HALT ===
        .ORIG x0138          ; Start TRAP_HALT handler at x0138
TRAP_HALT
        LEA R0, HALT_MSG     ; Load halt message address
        JSR TRAP_PUTS        ; Output halt message
        HALT                 ; Halt machine

; === Data Section ===
        .ORIG x0140          ; Data section start
KBSR    .FILL xFE00          ; Keyboard status register (memory-mapped I/O)
KBDR    .FILL xFE02          ; Keyboard data register address
DSR     .FILL xFE04          ; Display status register address
DDR     .FILL xFE06          ; Display data register address

TEMP_OUT .BLKW 1             ; Temporary storage for output char
SAVE_R0  .BLKW 1             ; Save R0 temporarily
SAVE_R1  .BLKW 1             ; Save R1 temporarily

PROMPT   .STRINGZ "Input a character: "  ; Prompt string
HALT_MSG .STRINGZ "HALT called.\n"       ; Halt message string

; === Main Program ===
        .ORIG x3000          ; Main program start
        LD R0, CHAR_A        ; Load 'A' ASCII code into R0
        TRAP x21             ; Output 'A'
        LEA R0, HELLO_MSG    ; Load hello message address
        TRAP x20             ; Wait for a char input (GETC)
        TRAP x23             ; Prompt input, get char, echo (IN)
        TRAP x25             ; Halt program (HALT)

CHAR_A  .FILL x0041          ; ASCII 'A'
HELLO_MSG .STRINGZ " Hello from LC3 TRAP test!\n"  ; Hello message

        .END                 ; End of program
