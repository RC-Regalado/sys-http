.section .text
.global _start
.type _start, @function
_start:
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

.global setsockopt
.type setsockopt, @function
setsockopt:
  mov %rdi, %rdi
  mov %rsi, %r10
  mov %rdx, %r8

  mov $54, %rax
  mov $1, %rsi
  mov $2, %rdx

  syscall
  ret
