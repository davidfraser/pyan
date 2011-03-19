    .file "closure.s"
    .text

# A full closure consists:
# The stub code
# The size of the closure
# The closure data
# A pointer to the target function

.globl make_closure
make_closure:
    # Enter
    push %ebp
    mov %esp, %ebp
    push %esi
    push %edi
    
    # Allocate a block of size closure_size + 8 + (stub2 - stub)
    movl 8(%ebp), %eax
    addl $8, %eax
    addl $(stub2 - stub), %eax
    pushl %eax
    call rt_malloc_exec
    
    # Copy stub into block
    movl $(stub2 - stub), %ecx
    sarl $2, %ecx
    movl $(stub), %esi
    movl %eax, %edi
    cld
    rep movsl
    
    # Copy closure into block as (size, data, pointer)
    movl 8(%ebp), %ecx
    addl $8, %ecx
    sarl $2, %ecx
    movl %ebp, %esi
    addl $8, %esi
    cld
    rep movsl
    
    # Print
    #movl %eax, %ebx
    #pushl %eax
    #call print_closure
    #movl %ebx, %eax
    
    # Exit
    pop %edi
    pop %esi
    leave
    ret


.align 4
stub:
    movl $trampoline, %eax
    call *%eax
    .align 4
stub2:


.globl trampoline
trampoline:
    # Find location of closure
    movl (%esp), %eax
    addl $3, %eax
    andl $0xFFFFFFFC, %eax
    
    # Fix stack and save return pointer
    movl 4(%esp), %ecx
    subl (%eax), %esp
    addl $8, %esp
    pushl %ecx
    
    # Copy closure onto stack
    movl (%eax), %ecx
    sarl $2, %ecx
    movl %eax, %esi
    addl $4, %esi
    movl %esp, %edi
    addl $4, %edi
    cld
    rep movsl
    
    # Jump to function
    movl (%esi), %eax
    jmp *%eax


.globl stub_size
	.data
	.align 4
	.type	stub_size, @object
	.size	stub_size, 4
stub_size:
	.long	stub2 - stub
