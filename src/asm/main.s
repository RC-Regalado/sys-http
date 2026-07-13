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

.type socketopt, @function
socketopt:
  mov %rdi, %rdi
  mov %rsi, %r10

  mov $1, %rsi
  syscall 
  ret

.global setsockopt
.type setsockopt, @function
setsockopt:
  mov %rdx, %r8
  mov $54, %rax
  mov $2, %rdx

  call socketopt
  ret

.global _getsockopt
.type _getsockopt, @function
_getsockopt:
  mov %rdx, %r8
  mov $55, %rax
  mov $4, %rdx

  call socketopt

  ret
