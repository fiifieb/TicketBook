// RISC-V RV64 optimized routines (optional). These override weak C fallbacks
// when compiled on a riscv64 target. Safe to exclude on other architectures.

    .text
    .option nopic

// int tb_memcmp_token32(const void *a, const void *b, size_t n)
// Returns 0 if equal, non-zero otherwise. Compares up to 32 bytes.
    .globl tb_memcmp_token32
    .type  tb_memcmp_token32, @function
tb_memcmp_token32:
    // a0 = a, a1 = b, a2 = n
    // cap n at 32
    li      t0, 32
    bgeu    a2, t0, 1f
    j       2f
1:
    mv      a2, t0
2:
    // if n == 0 return 0
    beqz    a2, .Lret0

    // Process 8-byte chunks while n >= 8
.Lword_loop:
    li      t1, 8
    bltu    a2, t1, .Lbyte_loop
    ld      t2, 0(a0)
    ld      t3, 0(a1)
    xor     t2, t2, t3
    bnez    t2, .Lret1
    addi    a0, a0, 8
    addi    a1, a1, 8
    addi    a2, a2, -8
    j       .Lword_loop

// Tail compare byte-by-byte
.Lbyte_loop:
    beqz    a2, .Lret0
    lbu     t2, 0(a0)
    lbu     t3, 0(a1)
    bne     t2, t3, .Lret1
    addi    a0, a0, 1
    addi    a1, a1, 1
    addi    a2, a2, -1
    j       .Lbyte_loop

.Lret0:
    li      a0, 0
    ret
.Lret1:
    li      a0, 1
    ret

    .size tb_memcmp_token32, .-tb_memcmp_token32

