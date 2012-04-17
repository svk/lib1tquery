#ifdef USE_MERTAP_ASSEMBLY

.globl mertap_cmp_asm1
.globl mertap_cmp_asm2
.globl mertap_cmp_asm3
.globl mertap_cmp_asm4
.globl mertap_cmp_asm5
mertap_cmp_asm5:
    movl (%rdi), %eax
    subl (%rsi), %eax
    jnz mertap_cmp_asm_end
    add $4, %rdi
    add $4, %rsi
mertap_cmp_asm4:
    movl (%rdi), %eax
    subl (%rsi), %eax
    jnz mertap_cmp_asm_end
    add $4, %rdi
    add $4, %rsi
mertap_cmp_asm3:
    movl (%rdi), %eax
    subl (%rsi), %eax
    jnz mertap_cmp_asm_end
    add $4, %rdi
    add $4, %rsi
mertap_cmp_asm2:
    movl (%rdi), %eax
    subl (%rsi), %eax
    jnz mertap_cmp_asm_end
    add $4, %rdi
    add $4, %rsi
mertap_cmp_asm1:
    movl (%rdi), %eax
    subl (%rsi), %eax
mertap_cmp_asm_end:
    ret

#endif
