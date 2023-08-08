
extern MmCopyMemory : proto
extern ReadPhysicalAddress : proto

.code

TranslateAddress PROC
	push rbp
	mov rbp, rsp
	push rax
	push rdx

	mov rax, cr3		; Get our cr3 physical address
	shr rax, 12
	shl rax, 12

	shr rcx, 39
	and rcx, 511		; 0x1ff = 111111111 i.e mask first 9 bits

	push rax			; rax is used for multiplication, save it
	mov rax, rcx		
	mov r10, 8
	mul r10				; multiply our pml4 index by 8
	mov rcx, rax
	pop rax
	add rcx, rax		; Add our cr3 physical with our pml4 offset

	sub rsp, 8
	lea rdx, [rsp + 8]	; Destination for pml4e after read
	mov r8, 8			; length of copy 
	call ReadPhysicalAddress

	add rsp, 8
	pop rdx
	pop rbp
	ret
TranslateAddress ENDP

END