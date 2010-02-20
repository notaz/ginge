.text

/* void do_entry(Elf32_Addr entry, void *stack_frame, int stack_frame_size, void *exitf); */

.globl do_entry
do_entry:
	movl	4(%esp), %ebx
	movl	8(%esp), %esi
	movl	12(%esp), %eax
	movl	16(%esp), %edx

	movl    %eax, %ecx
	shll    $2, %ecx
	subl    %ecx, %esp

	/* copy stack frame */
	movl	%esp, %edi
0:
	movl	(%esi), %ecx
	movl	%ecx, (%edi)
	addl    $4, %esi
	addl    $4, %edi
	subl	$1, %eax
	jg	0b

	jmp	*%ebx
