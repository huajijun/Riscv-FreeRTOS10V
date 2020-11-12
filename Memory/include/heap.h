#include <common.h>
#define heapMINIMUM_BLOCK_SIZE ( ( size_t ) ( heapSTRUCT_SIZE * 2 ) )
void *pvPortMalloc( size_t xWantedSize );
void vPortFree( void *pv );
