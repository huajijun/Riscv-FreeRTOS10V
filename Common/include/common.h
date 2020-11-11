#define portMAX_DELAY ( TickType_t ) 0xffffffffUL
#define uint32_t unsigned int
typedef unsigned long UBaseType_t;
typedef uint32_t TickType_t;
#define int8_t  char
#define uint8_t  unsigned char
#define uint16_t short 
#define uint32_t unsigned int
#define uint64_t unsigned long
typedef long BaseType_t;

#define errQUEUE_EMPTY                           ( ( BaseType_t ) 0 )
#define errQUEUE_FULL                            ( ( BaseType_t ) 0 )
#define StackType_t uint32_t
#define portPRIVILEGE_BIT ( ( UBaseType_t ) 0x00 )
#define pdFALSE false
#define pdTRUE  true
#define pdPASS          ( pdTRUE )

#define size_t int
#define configMAX_PRIORITIES        ( 30 )
#define portPOINTER_SIZE_TYPE   int
#define configMAX_TASK_NAME_LEN         ( 16 )
#define configINITIAL_TICK_COUNT    0
#define errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY   ( -1 )
#define portTASK_FUNCTION( vFunction, pvParameters ) void vFunction( void *pvParameters )
