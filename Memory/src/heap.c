#include <heap.h>
#define portBYTE_ALIGNMENT	8
#define configTOTAL_HEAP_SIZE		( ( size_t ) ( 100 * 1024 ) )
#define portBYTE_ALIGNMENT_MASK ( 0x0007 )

typedef struct A_BLOCK_LINK
{
	struct A_BLOCK_LINK *pxNextFreeBlock;	/*<< The next free block in the list. */
	size_t xBlockSize;						/*<< The size of the free block. */
} BlockLink_t;


/* Create a couple of list links to mark the start and end of the list. */
static BlockLink_t xStart, xEnd;


/* Allocate the memory for the heap. */
static uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];



/* A few bytes might be lost to byte aligning the heap start address. */
#define configADJUSTED_HEAP_SIZE	( configTOTAL_HEAP_SIZE - portBYTE_ALIGNMENT )

static size_t xFreeBytesRemaining = configADJUSTED_HEAP_SIZE;

/*#ifdef __riscv64
	#define portBYTE_ALIGNMENT	8
#else
	#define portBYTE_ALIGNMENT	4
#endif*/


//#define portPOINTER_SIZE_TYPE	uint32_t

#define prvInsertBlockIntoFreeList( pxBlockToInsert )								\
{																					\
BlockLink_t *pxIterator;																\
size_t xBlockSize;																	\
																					\
	xBlockSize = pxBlockToInsert->xBlockSize;										\
																					\
	/* Iterate through the list until a block is found that has a larger size */	\
	/* than the block we are inserting. */											\
	for( pxIterator = &xStart; pxIterator->pxNextFreeBlock->xBlockSize < xBlockSize; pxIterator = pxIterator->pxNextFreeBlock )	\
	{																				\
		/* There is nothing to do here - just iterate to the correct position. */	\
	}																				\
																					\
	/* Update the list to include the block being inserted in the correct */		\
	/* position. */																	\
	pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;					\
	pxIterator->pxNextFreeBlock = pxBlockToInsert;									\
}


static const uint16_t heapSTRUCT_SIZE	= ( ( sizeof ( BlockLink_t ) + ( portBYTE_ALIGNMENT - 1 ) ) & ~portBYTE_ALIGNMENT_MASK );
static void prvHeapInit( void )
{
	BlockLink_t* pxFirstFreeBlock;
	uint8_t *pucAlignedHeap;
	pucAlignedHeap = (uint8_t*)(((portPOINTER_SIZE_TYPE)&ucHeap[portBYTE_ALIGNMENT])&(~((portPOINTER_SIZE_TYPE)portBYTE_ALIGNMENT_MASK ) ) );
	
	xStart.pxNextFreeBlock = (void*)pucAlignedHeap;
	xStart.xBlockSize = 0;

	xEnd.pxNextFreeBlock = NULL;
	xEnd.xBlockSize = configADJUSTED_HEAP_SIZE;

	/* To start with there is a single free block that is sized to take up the
	entire heap space. */
	pxFirstFreeBlock = ( void * ) pucAlignedHeap;
	pxFirstFreeBlock->xBlockSize = configADJUSTED_HEAP_SIZE;
	pxFirstFreeBlock->pxNextFreeBlock = &xEnd;

}


void *pvPortMalloc( size_t xWantedSize )
{
	BlockLink_t *pxBlock, *pxPreviousBlock, *pxNewBlockLink;
	static bool xHeapHasBeenInitialised = false;
	void *pvReturn = NULL;
	/* init memory if not */
	if( xHeapHasBeenInitialised == false)
	{
		prvHeapInit();
		xHeapHasBeenInitialised = true;
	}


	/* address align for xWantedSize */
	if( xWantedSize > 0 )
	{
		xWantedSize += heapSTRUCT_SIZE;

		/* Ensure that blocks are always aligned to the required number of bytes. */
		if( ( xWantedSize & portBYTE_ALIGNMENT_MASK ) != 0 )
		{
			/* Byte alignment required. */
			xWantedSize += ( portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK ) );
		}
	}


	if( ( xWantedSize > 0 ) && ( xWantedSize < configADJUSTED_HEAP_SIZE ) )
	{
		/* Blocks are stored in byte order - traverse the list from the start
		(smallest) block until one of adequate size is found. */
		pxPreviousBlock = &xStart;
		pxBlock = xStart.pxNextFreeBlock;
		while( ( pxBlock->xBlockSize < xWantedSize ) && ( pxBlock->pxNextFreeBlock != NULL ) )
		{
			pxPreviousBlock = pxBlock;
			pxBlock = pxBlock->pxNextFreeBlock;
		}

		/* If we found the end marker then a block of adequate size was not found. */
		if( pxBlock != &xEnd )
		{
			/* Return the memory space - jumping over the BlockLink_t structure
			at its start. */
			pvReturn = ( void * ) ( ( ( uint8_t * ) pxPreviousBlock->pxNextFreeBlock ) + heapSTRUCT_SIZE );

			/* This block is being returned for use so must be taken out of the
			list of free blocks. */
			pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

			/* If the block is larger than required it can be split into two. */
			if( ( pxBlock->xBlockSize - xWantedSize ) > heapMINIMUM_BLOCK_SIZE )
			{
				/* This block is to be split into two.  Create a new block
				following the number of bytes requested. The void cast is
				used to prevent byte alignment warnings from the compiler. */
				pxNewBlockLink = ( void * ) ( ( ( uint8_t * ) pxBlock ) + xWantedSize );

				/* Calculate the sizes of two blocks split from the single
				block. */
				pxNewBlockLink->xBlockSize = pxBlock->xBlockSize - xWantedSize;
				pxBlock->xBlockSize = xWantedSize;

				/* Insert the new block into the list of free blocks. */
				prvInsertBlockIntoFreeList( ( pxNewBlockLink ) );
			}

			xFreeBytesRemaining -= pxBlock->xBlockSize;
		}
	}


	return pvReturn;

}
void vPortFree( void *pv )
{
	uint8_t *puc = ( uint8_t * ) pv;
	BlockLink_t *pxLink;
	if( pv != NULL )
	{
		/* The memory being freed will have an BlockLink_t structure immediately
		before it. */
		puc -= heapSTRUCT_SIZE;

		/* This unexpected casting is to keep some compilers from issuing
		byte alignment warnings. */
		pxLink = ( void * ) puc;
		{
			/* Add this block to the list of free blocks. */
			prvInsertBlockIntoFreeList( ( ( BlockLink_t * ) pxLink ) );
			xFreeBytesRemaining += pxLink->xBlockSize;
		}
	}
}


