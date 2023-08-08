
extern MmCopyMemory : proto
extern ReadPhysicalAddress : proto

.code

TranslateAddress PROC

	push rbp
	mov rbp, rsp
	push rax
	push rdx
	push rcx			; store the virtual address on the stack

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

	mov rbx, [rsp + 8]	; move result into rbx
	bt rbx, 0			; check if the present bit is set
	sbb rbx, rbx		; rbx will be 0xffff.. if its set and 0x000.. if its not set
	test rbx, rbx
	je _end				; if the present bit is not set, jump to end

	mov rbx, [rsp + 8]	; get the result back can definitely do this more efficiently lol
	shr rbx, 12		
	shl rbx, 24
	shr rbx, 12			; extract the next physical base address from our pml4e
	mov rdi, [rsp + 72]
	shr rdi, 30			
	and rdi, 511		; same thing as before, extract the pdpte offset from the virtual address

	push rax			
	mov rax, rdi
	mov r10, 8
	mul r10				; multiply our pdpt index by 8
	mov rdi, rax
	pop rax
	add rdi, rbx		; add our pml4e and pdpt offset

	mov rcx, rdi		; setup arguments for 2nd translation
	sub rsp, 8
	lea rdx, [rsp + 8]
	mov r8, 8
	call ReadPhysicalAddress

_end:
	add rsp, 16
	pop rcx
	pop rdx
	pop rax
	pop rbp
	ret

TranslateAddress ENDP

END