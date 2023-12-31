#include <ntifs.h>
#include <wdftypes.h>
#include <wdf.h>

#include <intrin.h>

#define DEBUG_LOG(fmt, ...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "[+] " fmt "\n", ##__VA_ARGS__)
#define DEBUG_ERROR(fmt, ...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "[-] " fmt "\n", ##__VA_ARGS__)

extern UINT64 TranslateAddress(INT* Number);

NTSTATUS ReadPhysicalAddress(
	_In_ UINT64 Source,
	_In_ PVOID Destination,
	_In_ UINT32 Length
)
{
	DEBUG_LOG( "ReadPhysicalAddress Params: Source: %llx, Destination %llx, Length %lx", Source, (UINT64)Destination, Length );
	SIZE_T bytes_copied;
	MM_COPY_ADDRESS copy_address = { 0 };

	//Store our 64 bit address in the QuadPart of the physical address struct
	copy_address.PhysicalAddress.QuadPart = Source;

	if ( !NT_SUCCESS( MmCopyMemory(
		Destination,
		copy_address,
		Length,
		MM_COPY_MEMORY_PHYSICAL,
		&bytes_copied
	) ) )
	{
		return STATUS_ABANDONED;
	};

	DEBUG_LOG( "success: %llx", (UINT64)Destination );
	return STATUS_SUCCESS;

}

typedef union _DIRECTORY_TABLE_BASE
{
    struct
    {
        UINT64 Ignored0 : 3;            /* 2:0   */
        UINT64 PageWriteThrough : 1;    /* 3     */
        UINT64 PageCacheDisable : 1;    /* 4     */
        UINT64 _Ignored1 : 7;           /* 11:5  */
        UINT64 PhysicalAddress : 36;    /* 47:12 */
        UINT64 _Reserved0 : 16;         /* 63:48 */

    } Bits;

    UINT64 BitAddress;

} CR3, DIR_TABLE_BASE;

typedef union _VIRTUAL_MEMORY_ADDRESS
{
	struct
	{
		UINT64 PageIndex : 12;  /* 0:11  */
		UINT64 PtIndex : 9;	/* 12:20 */
		UINT64 PdIndex : 9;	/* 21:29 */
		UINT64 PdptIndex : 9;   /* 30:38 */
		UINT64 Pml4Index : 9;   /* 39:47 */
		UINT64 Unused : 16;	/* 48:63 */

	} Bits;

	UINT64 BitAddress;

} VIRTUAL_ADDRESS, * PVIRTUAL_ADDRESS;

typedef union _PML4_ENTRY
{
	struct
	{
		UINT64 Present : 1;    /* 0     */
		UINT64 ReadWrite : 1;    /* 1     */
		UINT64 UserSupervisor : 1;    /* 2     */
		UINT64 PageWriteThrough : 1;    /* 3     */
		UINT64 PageCacheDisable : 1;    /* 4     */
		UINT64 Accessed : 1;    /* 5     */
		UINT64 _Ignored0 : 1;    /* 6     */
		UINT64 _Reserved0 : 1;    /* 7     */
		UINT64 _Ignored1 : 4;    /* 11:8  */
		UINT64 PhysicalAddress : 40;   /* 51:12 */
		UINT64 _Ignored2 : 11;   /* 62:52 */
		UINT64 ExecuteDisable : 1;    /* 63    */
	} Bits;
	UINT64 All;
} PML4E;

typedef union _PDPT_ENTRY
{
	struct
	{
		UINT64 Present : 1;    /* 0     */
		UINT64 ReadWrite : 1;    /* 1     */
		UINT64 UserSupervisor : 1;    /* 2     */
		UINT64 PageWriteThrough : 1;    /* 3     */
		UINT64 PageCacheDisable : 1;    /* 4     */
		UINT64 Accessed : 1;    /* 5     */
		UINT64 _Ignored0 : 1;    /* 6     */
		UINT64 PageSize : 1;    /* 7     */
		UINT64 _Ignored1 : 4;    /* 11:8  */
		UINT64 PhysicalAddress : 40;   /* 51:12 */
		UINT64 _Ignored2 : 11;   /* 62:52 */
		UINT64 ExecuteDisable : 1;    /* 63    */
	} Bits;
	UINT64 All;
} PDPTE;

typedef union _PD_ENTRY
{
	struct
	{
		UINT64 Present : 1;    /* 0     */
		UINT64 ReadWrite : 1;    /* 1     */
		UINT64 UserSupervisor : 1;    /* 2     */
		UINT64 PageWriteThrough : 1;    /* 3     */
		UINT64 PageCacheDisable : 1;    /* 4     */
		UINT64 Accessed : 1;    /* 5     */
		UINT64 _Ignored0 : 1;    /* 6     */
		UINT64 PageSize : 1;    /* 7     */
		UINT64 _Ignored1 : 4;    /* 11:8  */
		UINT64 PhysicalAddress : 38;   /* 49:12 */
		UINT64 _Reserved0 : 2;    /* 51:50 */
		UINT64 _Ignored2 : 11;   /* 62:52 */
		UINT64 ExecuteDisable : 1;    /* 63    */
	} Bits;
	UINT64 All;
} PDE;

typedef union _PT_ENTRY
{
	struct
	{
		UINT64 Present : 1;    /* 0     */
		UINT64 ReadWrite : 1;    /* 1     */
		UINT64 UserSupervisor : 1;    /* 2     */
		UINT64 PageWriteThrough : 1;    /* 3     */
		UINT64 PageCacheDisable : 1;    /* 4     */
		UINT64 Accessed : 1;    /* 5     */
		UINT64 Dirty : 1;    /* 6     */
		UINT64 PageAttributeTable : 1;    /* 7     */
		UINT64 Global : 1;    /* 8     */
		UINT64 _Ignored0 : 3;    /* 11:9  */
		UINT64 PhysicalAddress : 38;   /* 49:12 */
		UINT64 _Reserved0 : 2;    /* 51:50 */
		UINT64 _Ignored1 : 7;    /* 58:52 */
		UINT64 ProtectionKey : 4;    /* 62:59 */
		UINT64 ExecuteDisable : 1;    /* 63    */
	} Bits;
	UINT64 All;
} PTE;

void test(int* number)
{
	DEBUG_LOG( "--------------------------TEST-----------------------------" );
    CR3 cr3 = { 0 };
    cr3.BitAddress = __readcr3();
    DEBUG_LOG( "Cr3: %llx, Physical Subpart: %llx, shifted: %llx", 
        cr3.BitAddress, 
        cr3.Bits.PhysicalAddress, 
        ( cr3.Bits.PhysicalAddress << 12 ) );

	VIRTUAL_ADDRESS virtual = { 0 };
	virtual.BitAddress = number;
	DEBUG_LOG( "address: %llx, pml4 index: %llx", 
		virtual.BitAddress, 
		virtual.Bits.Pml4Index );

	DEBUG_LOG( "cr3 + pml4: %llx", ( cr3.Bits.PhysicalAddress << 12 ) + virtual.Bits.Pml4Index * 8 );

	PML4E pml4e = { 0 };
	ReadPhysicalAddress( ( cr3.Bits.PhysicalAddress << 12 ) + (virtual.Bits.Pml4Index * 8), &pml4e, sizeof(PML4E));

	DEBUG_LOG( "pml4 physical: %llx, pml4e phys shifted %llx, virtual pdpt index: %llx, added together: %llx", 
		pml4e.Bits.PhysicalAddress,
		( pml4e.Bits.PhysicalAddress << 12 ), 
		( virtual.Bits.PdptIndex ), 
		( pml4e.Bits.PhysicalAddress << 12 ) + virtual.Bits.PdptIndex * 8 );

	PDPTE pdpte = { 0 };
	ReadPhysicalAddress( ( pml4e.Bits.PhysicalAddress << 12 ) + ( virtual.Bits.PdptIndex * 8 ), &pdpte, sizeof( PDPTE ) );

	DEBUG_LOG("pdpte physical: %llx, pdpte phys shifted %llx, virtual pd index %llx, added toghether: llx",
		pdpte.Bits.PhysicalAddress,
		(pdpte.Bits.PhysicalAddress << 12 ),
		virtual.Bits.PdIndex ),
		( pdpte.Bits.PhysicalAddress << 12 ) + (virtual.Bits.PdIndex * 8);

	PDE pde = { 0 };
	ReadPhysicalAddress( ( pdpte.Bits.PhysicalAddress << 12 ) + ( virtual.Bits.PdIndex * 8 ), &pde, sizeof(PDE ));

	DEBUG_LOG( "pde physical: %llx, pde phys shifted %llx, virtual pt index %llx, added together: %llx",
		pde.Bits.PhysicalAddress,
		( pde.Bits.PhysicalAddress << 12 ),
		virtual.Bits.PtIndex,
		( pde.Bits.PhysicalAddress << 12 ) + ( virtual.Bits.PtIndex * 8 ) );


	PTE pte = { 0 };
	ReadPhysicalAddress( ( pde.Bits.PhysicalAddress << 12 ) + ( virtual.Bits.PtIndex * 8 ), &pte, sizeof( PTE ));
	UINT64 physical_address = ( pte.Bits.PhysicalAddress << 12 ) + ( virtual.Bits.PageIndex );

	DEBUG_LOG( "Physical from c function: %llx", physical_address );

	DEBUG_LOG( "-------------------------TEST---------------------------" );
}

void bextr_test( int* number )
{
	DEBUG_LOG( "----------------BEXTR TEST --------------------------" );

	UINT64 virtual_address = (UINT64)number;

	UINT64 cr3 = __readcr3();
	UINT64 cr3_physical = _bextr_u64( cr3, 12, 36 ) << 12;
	UINT64 pml4 = _bextr_u64( virtual_address, 39, 9 ) * 8;
	UINT64 pml4e = 0;
	DEBUG_LOG( "cr3 physical: %llx, pml4 entry: %llx", cr3_physical, pml4 );
	ReadPhysicalAddress( cr3_physical + pml4, &pml4e, sizeof( UINT64 ) );


		DEBUG_LOG( "----------------BEXTR TEST --------------------------" );
}

NTSTATUS DriverEntry(
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PUNICODE_STRING RegistryPath
)
{
	//Note: rewrite the c using the BEXTR instruction
	UNREFERENCED_PARAMETER( RegistryPath );
	INT number = 420;
	DEBUG_LOG( "number addr: %llx", (UINT64 ) &number);
	test( &number );
	bextr_test( &number );
	//UINT64 test = TranslateAddress(&number);
	//DEBUG_LOG( "Translation -- Virtual: %llx, Physical: %llx", (UINT64)&number, test);
	__debugbreak();
	return STATUS_SUCCESS;
}