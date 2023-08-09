
extern MmCopyMemory : proto
extern ReadPhysicalAddress : proto

.code

TranslateAddress PROC

	push rbp					; setup stack frame, dont really need to store rbp on the stack on x64
	mov rbp, rsp
	sub rsp, 32

	mov rax, cr3				; Get our cr3 physical address
	shr rax, 12
	shl rax, 12
	shr rcx, 39					; pml4 index begins at the 39th bit (39:47)
	and rcx, 511				; 0x1ff = 111111111 i.e mask first 9 bits

	push rax					; rax is used for multiplication, save it
	mov rax, rcx		
	mov r10, 8
	mul r10						; multiply our pml4 index by 8
	mov rcx, rax
	pop rax
	add rcx, rax				; Add our cr3 physical with our pml4 offset

	lea rdx, [rsp + 8]			; Destination for pml4e after read
	mov r8, 8					; length of copy 
	call ReadPhysicalAddress

	mov rbx, [rsp + 8]			; move result into rbx
	bt rbx, 0					; check if the present bit is set
	sbb rbx, rbx				; rbx will be 0xffff.. if its set and 0x000.. if its not set
	test rbx, rbx
	je _end						; if the present bit is not set, jump to end

	mov rbx, [rsp + 8]			; get the result back can definitely do this more efficiently lol
	shr rbx, 12		
	shl rbx, 24
	shr rbx, 12					; extract the next physical base address from our pml4e
	mov rdi, [rsp + 48]
	shr rdi, 30			
	and rdi, 511				; same thing as before, extract the pdpte offset from the virtual address

	push rax					; save rax while we multiply
	mov rax, rdi
	mov r10, 8
	mul r10						; multiply our pdpt index by 8
	mov rdi, rax
	pop rax
	add rdi, rbx				; add our pml4e and pdpt offset

	mov rcx, rdi				; setup arguments for 2nd translation
	lea rdx, [rsp + 16]
	mov r8, 8
	call ReadPhysicalAddress

	mov rcx, [rsp + 16]			; move destination value into rcx
	bt rcx, 0					; validate the present bit is set
	sbb rcx, rcx
	test rcx, rcx
	je _end						; if its not set, jump to end

	mov rcx, [rsp + 16]
	shr rcx, 6					; 7th bit tells us whether this pdpt entry points to a large page
	bt rcx, 0
	sbb rcx, rcx
	test rcx, rcx
	je _LARGE_PAGE_1GB				

	mov rcx, [rsp + 16]
	shr rcx, 12					; extract the physical base address from our pdpte
	shl rcx, 24
	shr rcx, 12		
	mov rdi, [rsp + 48]			; extract our pdpte offset from the virtual address
	shr rdi, 21
	and rdi, 511

	push rax					; multiply our pdpte inedex by 8
	mov rax, rdi
	mov r10, 8
	mul r10
	mov rdi, rax
	pop rax
	add rdi, rcx				; add our pdpte base and pdpte index

	mov rcx, rdi				; setup our arguments with the new values
	lea rdx, [rsp + 24]
	mov r8, 8
	call ReadPhysicalAddress

	mov rcx, [rsp + 24]			; test for present bit
	bt rcx, 0
	sbb rcx, rcx
	test rcx, rcx
	je _end						

	mov rcx, [rsp + 24]			; test to see if it points to a 2MB page
	shr rcx, 6
	bt rcx, 0
	sbb rcx, rcx
	test rcx, rcx
	je _LARGE_PAGE_2MB			; it does, jump to 2mb page extraction

	mov rcx, [rsp + 24]			; Now we have our PDE
	shr rcx, 12
	shl rcx, 26
	shr rcx, 14					; extract the physical (49:12)
	mov rdi, [rsp + 48]			; extract the table offset from virtual
	shr rdi, 12
	and rdi, 511

	push rax					; multiply and add our offsets
	mov rax, rdi
	mov r10, 8
	mul r10
	mov rdi, rax
	pop rax
	add rdi, rcx

	mov rcx, rdi				; setup arguments
	lea rdx, [rsp + 24]
	mov r8, 8
	call ReadPhysicalAddress

	mov rcx, [rsp + 24]			; we now have our base pte
	bt rcx, 0
	sbb rcx, rcx
	test rcx, rcx
	je _end

	mov rcx, [rsp + 24]			; extract our physical from the pte
	shr rcx, 12
	shl rcx, 26
	shr rcx, 14
	mov rdi, [rsp + 48]			; extract the page index from our virtual address
	and rdi, 4095				; 111111111111 mask the first 12 bits
	add rcx, rdi				; add them to get our final physical address

	mov rax, rcx				; insert address into rax which holds the return value
	jmp _end

_LARGE_PAGE_1GB:
	mov rcx, [rsp + 16]			; extract our physical address from our pdpte
	shr rcx, 30
	shl rcx, 30
	shl rcx, 12
	shr rcx. 12
	shr rcx, 30
	mov rdi, [rsp + 48]			; extract our pd index from our virtual address
	shr rdi, 21
	and rdi, 511
	push rax					; multiply our offset by 8 and add it to our pdpte 
	mov rax, rdi
	mov r10, 8
	mul r10
	mov rdi, rax
	pop rax
	add rdi, rcx
	mov rax, rdi				; move final physical address into rax and jump to cleanup
	jmp _end

_LARGE_PAGE_2MB:
	mov rcx, [rsp + 24]			; extract our physical address from our PDE
	shr rcx, 21
	shl rcx, 30
	shl rcx, 5
	shr rcx, 30
	shr rcx, 5
	mov rdi, [rsp + 48]			; extract our pt index and multiply it by 8
	shr rdi, 12
	and rdi, 511
	push rax
	mov rax, rdi
	mov r10, 8
	mul r10
	mov rdi, rax
	pop rax
	add rdi, rcx
	mov rax, rdi				; store final address in rax and jump to end
	jmp _end

_end:
	add rsp, 32					; restore stack
	pop rbp
	ret

TranslateAddress ENDP

END