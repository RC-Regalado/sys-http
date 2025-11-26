.section .text
.global sys_epoll_create1
.type sys_epoll_create1, @function
sys_epoll_create1:
  mov $291, %rax  #SYS_EPOLL_CREATE1
  mov $0, %rdi

  syscall
  ret

.global sys_epoll_ctl
.type sys_epoll_ctl, @function
sys_epoll_ctl:
  mov $233, %rax  #sys_epoll_ctl

  # Se asume que los parametros se ordenan de la misma forma en
  # que la syscall lo espera
  syscall
  ret

.global sys_epoll_wait
.type sys_epoll_wait, @function
sys_epoll_wait:
  mov $232, %rax  #SYS_EPOLL_WAIT
  mov %rdi, %rdi
  mov %rsi, %rsi
  mov %rdx, %rdx 
  mov $-1, %r10

  syscall
  ret


