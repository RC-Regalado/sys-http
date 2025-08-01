.section .text
.global _start
.type _start, @function
_start:
//    xor %rbp, %rbp
//    and $-16, %rsp
//    sub $8, %rsp
    call server

    mov $60, %rax   # syscall: exit
    xor %rdi, %rdi  # status = 0
    syscall

.extern server
.global syscall3
.type syscall3, @function
syscall3:
    mov %rdi, %rax  # syscall number
    mov %rsi, %rdi  # arg1
    mov %rdx, %rsi  # arg2
    mov %rcx, %rdx  # arg3
    syscall
    ret

