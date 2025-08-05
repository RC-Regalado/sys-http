.section .text
.global sysalloc
.type sysalloc, @function
sysalloc:
    mov %rdi, %rsi  # Primer parámetro como segundo argumento de syscall
    mov $9, %rax    # sys_mmap
    mov $0, %rdi    # addr (Decide el kernel = 0)
    mov $3, %rdx    # PROT_READ | PROT_WRITE
    mov $0x22, %r10 # MAP_ANONYMUS | MAP_PRIVATE
    mov $-1, %r8    # Sin file descriptor
    mov $0, %r9     # Offset 

    syscall
    ret
