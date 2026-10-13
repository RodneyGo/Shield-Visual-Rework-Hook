EXTERN MissileProbeEntry:PROC
PUBLIC MissileFixture
PUBLIC MissileFixtureSite
.code
MissileFixture PROC FRAME
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    sub rsp, 80h
    .allocstack 80h
    .endprolog
    mov r12, rcx
    mov rdi, rdx
    mov r13, r8
    movups xmm0, [r9]
    movups [rsp+40h], xmm0
    movups xmm0, [r9+10h]
    movups [rsp+50h], xmm0
    movups xmm0, [r9+20h]
    movups [rsp+60h], xmm0
    movups xmm0, [r9+30h]
    movups [rsp+70h], xmm0
    mov eax, 11
    mov ecx, 17
    mov edx, 23
    mov r8d, 29
    mov r9d, 31
    mov r10d, 37
    mov r11d, 41
    movd xmm0, eax
    cmp eax, 11
MissileFixtureSite LABEL BYTE
    nop
    nop
    nop
    nop
    nop
    nop
    jne failed
    add rax, rcx
    add rax, rdx
    add rax, r8
    add rax, r9
    add rax, r10
    add rax, r11
    movd ecx, xmm0
    add rax, rcx
    jmp done
failed:
    xor eax, eax
done:
    add rsp, 80h
    pop r13
    pop r12
    pop rdi
    ret
MissileFixture ENDP
END
