; Entry observer: preserve Windows x64 argument state and tail-jump to MinHook.
; Stack arguments and original return address remain in their original places.
EXTERN CaptureContact:PROC
EXTERN ProbeTrampoline:QWORD
PUBLIC ProbeEntry
EXTERN CaptureMissileContact:PROC
EXTERN MissileProbeTrampoline:QWORD
PUBLIC MissileProbeEntry
.code
ProbeEntry PROC FRAME
    pushfq
    .allocstack 8
    sub rsp, 120h
    .allocstack 120h
    .endprolog
    mov [rsp+20h], rax
    mov [rsp+28h], rcx
    mov [rsp+30h], rdx
    mov [rsp+38h], r8
    mov [rsp+40h], r9
    mov [rsp+48h], r10
    mov [rsp+50h], r11
    lea rax, [rsp+128h]
    mov [rsp+58h], rax
    movdqu [rsp+60h], xmm0
    movdqu [rsp+70h], xmm1
    movdqu [rsp+80h], xmm2
    movdqu [rsp+90h], xmm3
    movdqu [rsp+0a0h], xmm4
    movdqu [rsp+0b0h], xmm5
    stmxcsr [rsp+0c0h]
    lea rcx, [rsp+20h]
    call CaptureContact
    ldmxcsr [rsp+0c0h]
    movdqu xmm0, [rsp+60h]
    movdqu xmm1, [rsp+70h]
    movdqu xmm2, [rsp+80h]
    movdqu xmm3, [rsp+90h]
    movdqu xmm4, [rsp+0a0h]
    movdqu xmm5, [rsp+0b0h]
    mov rax, [rsp+20h]
    mov rcx, [rsp+28h]
    mov rdx, [rsp+30h]
    mov r8, [rsp+38h]
    mov r9, [rsp+40h]
    mov r10, [rsp+48h]
    mov r11, [rsp+50h]
    lea rsp, [rsp+120h]
    popfq
    jmp QWORD PTR [ProbeTrampoline]
ProbeEntry ENDP
; Mid-function observer: original RSP is 16-byte aligned at 6082e5.
MissileProbeEntry PROC FRAME
    pushfq
    .allocstack 8
    sub rsp, 128h
    .allocstack 128h
    .endprolog
    mov [rsp+20h], rax
    mov [rsp+28h], rcx
    mov [rsp+30h], rdx
    mov [rsp+38h], r8
    mov [rsp+40h], r9
    mov [rsp+48h], r10
    mov [rsp+50h], r11
    lea rax, [rsp+130h]
    mov [rsp+58h], rax
    movdqu [rsp+60h], xmm0
    movdqu [rsp+70h], xmm1
    movdqu [rsp+80h], xmm2
    movdqu [rsp+90h], xmm3
    movdqu [rsp+0a0h], xmm4
    movdqu [rsp+0b0h], xmm5
    stmxcsr [rsp+0c0h]
    mov rcx, r12
    mov rdx, rdi
    mov r8, r13
    call CaptureMissileContact
    ldmxcsr [rsp+0c0h]
    movdqu xmm0, [rsp+60h]
    movdqu xmm1, [rsp+70h]
    movdqu xmm2, [rsp+80h]
    movdqu xmm3, [rsp+90h]
    movdqu xmm4, [rsp+0a0h]
    movdqu xmm5, [rsp+0b0h]
    mov rax, [rsp+20h]
    mov rcx, [rsp+28h]
    mov rdx, [rsp+30h]
    mov r8, [rsp+38h]
    mov r9, [rsp+40h]
    mov r10, [rsp+48h]
    mov r11, [rsp+50h]
    lea rsp, [rsp+128h]
    popfq
    jmp QWORD PTR [MissileProbeTrampoline]
MissileProbeEntry ENDP
END
