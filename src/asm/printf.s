.section .data
buffer: .space 256    # buffer de salida temporal

.section .text
.global logf
.type logf, @function

logf:
    push %rbp
    mov %rsp, %rbp

    mov %rdi, %r8      # format string
    mov %rsi, %r9      # arg1
    mov %rdx, %r10     # arg2
    lea buffer(%rip), %r11   # dest ptr
    xor %rcx, %rcx     # arg counter
    xor %rax, %rax     # current char

.next_char:
    movzbq (%r8), %rax
    test %rax, %rax
    je .done

    cmp $'%', %al
    je .handle_percent

    mov %al, (%r11)
    inc %r11
    inc %r8
    jmp .next_char

.handle_percent:
    inc %r8
    movzbq (%r8), %rax
    cmp $'s', %al
    jne .skip_unknown

    cmp $0, %rcx
    je .use_arg1

    cmp $1, %rcx
    je .use_arg2

    jmp .skip_unknown

.use_arg1:
    lea (%r9), %rsi
    jmp .insert_arg

.use_arg2:
    lea (%r10), %rsi
    jmp .insert_arg

.insert_arg:
    .copy_loop:
        movzbq (%rsi), %rax
        test %rax, %rax
        je .copy_done

        mov %al, (%r11)
        inc %rsi
        inc %r11
        jmp .copy_loop
    .copy_done:
        inc %rcx
        inc %r8
        jmp .next_char

.skip_unknown:
    mov $'%', %al
    mov %al, (%r11)
    inc %r11
    mov (%r8), %al
    mov %al, (%r11)
    inc %r11
    inc %r8
    jmp .next_char

.done:
    movb $'\n', (%r11)
    inc %r11

    # len = r11 - buffer
    lea buffer(%rip), %rsi
    mov %rsi, %rdi
    mov %r11, %rax
    sub %rsi, %rax
    mov $1, %rdi         # fd = STDOUT
    mov %rax, %rdx       # count
    lea buffer(%rip), %rsi
    mov $1, %rax         # syscall write
    syscall

    pop %rbp
    ret

