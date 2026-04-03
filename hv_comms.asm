; hv_comms.asm - VMMCALL stub for hypervisor communication
;
; C++ prototype:
;   extern "C" uint64_t hv_vmmcall(uint64_t cmd, uint64_t auth,
;                                   uint64_t p1, uint64_t p2,
;                                   uint64_t p3, uint64_t p4);
;
; Win64 ABI input:
;   RCX = cmd, RDX = auth, R8 = p1, R9 = p2, [RSP+40] = p3, [RSP+48] = p4
;
; Hypervisor expects:
;   RAX = cmd, RCX = auth, RDX = p1, RBX = p2, RSI = p3, RDI = p4
;
; Returns: RAX = hypervisor result

section .text

global hv_vmmcall

hv_vmmcall:
    ; Save callee-saved registers
    push    rbx
    push    rsi
    push    rdi

    ; Shuffle registers to hypervisor convention
    mov     rax, rcx            ; RAX = cmd
    mov     rcx, rdx            ; RCX = auth
    mov     rdx, r8             ; RDX = p1
    mov     rbx, r9             ; RBX = p2
    mov     rsi, [rsp + 64]     ; RSI = p3 (40 + 24 from pushes)
    mov     rdi, [rsp + 72]     ; RDI = p4 (48 + 24 from pushes)

    ; VMMCALL instruction (0F 01 D9)
    db      0x0F, 0x01, 0xD9

    ; Result in RAX

    ; Restore callee-saved registers
    pop     rdi
    pop     rsi
    pop     rbx

    ret
