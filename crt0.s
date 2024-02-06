	.text

	.globl	_start
_start:
	movl  (%rsp), %edi
	lea   8(%rsp), %rsi
	lea   8(%rsi,%rdi,8), %rdx
	call  os_main
	movl  %eax, %edi
	movl  $60, %eax
	syscall

	.globl	memset
memset:
	movq	%rdx, %rcx
	movq	%rdi, %rdx
	movl	%esi, %eax
	rep stosb
	movq	%rdx, %rax
	ret

	.globl	memcpy
memcpy:
	movq	%rdi, %rax
	movq	%rdx, %rcx
	rep movsb
	ret

	.globl	memmove
memmove:
	movq	%rdi, %rax
	movq	%rdx, %rcx
	cmp	%rdi, %rsi
	ja	0f
	leaq	-1(%rdi,%rdx), %rdi
	leaq	-1(%rsi,%rdx), %rsi
	std
0:	rep movsb
	cld
	ret

	.globl	strlen
strlen:
	orq	$-1, %rcx
	xorl	%eax, %eax
	repne scasb
	movq	$-2, %rax
	subq	%rcx, %rax
	ret

	.globl	syscall1
syscall1:
	movq	%rsi, %rax
	syscall
	ret

	.globl	syscall2
syscall2:
	movq	%rdx, %rax
	syscall
	ret

	.globl	syscall3
syscall3:
	movq	%rcx, %rax
	syscall
	ret

	.globl	syscall6
syscall6:
	movq	%rcx, %r10
	movq	8(%rsp), %rax
	syscall
	ret
