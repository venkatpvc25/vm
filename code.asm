          .ORIG x3000
          START LD R1, VALUE ; comment here
        ADD R1, R1, #1
        ST R1, VALUE
        BRnzp START
VALUE   .FILL x000A
        .END
