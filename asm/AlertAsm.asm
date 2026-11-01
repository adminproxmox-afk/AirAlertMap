; Win32 / cdecl / OMF. Built with the C++Builder Turbo Assembler.
.386
.model flat
.code
PUBLIC _CountActiveAlerts
_CountActiveAlerts PROC
    mov edx, [esp+4]      ; states (caller-owned byte array)
    mov ecx, [esp+8]      ; signed number of elements
    xor eax, eax
    test edx, edx
    jz count_done
    test ecx, ecx
    jle count_done
count_loop:
    cmp byte ptr [edx], 2 ; asAlert
    jne next_state
    inc eax
next_state:
    inc edx
    dec ecx
    jnz count_loop
count_done:
    ret
_CountActiveAlerts ENDP
END
