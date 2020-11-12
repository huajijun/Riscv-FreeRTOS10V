#ifndef zzy
#define zzy
#include<FreeRTOS.h>
#define uint32_t unsigned int
typedef unsigned long   UBaseType_t;
typedef uint32_t TickType_t;
#define int8_t  char
#define uint8_t  unsigned char
#define uint16_t short 
#define uint32_t unsigned int
#define uint64_t unsigned long
typedef long BaseType_t;

#define bool	char
#define errQUEUE_EMPTY                           ( ( BaseType_t ) 0 )
#define errQUEUE_FULL                            ( ( BaseType_t ) 0 )
#define StackType_t uint32_t
#define portPRIVILEGE_BIT ( ( UBaseType_t ) 0x00 )
#define false 0
#define true 1
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
#define NULL    0
#define configSTACK_DEPTH_TYPE    uint16_t
#define portMAX_DELAY ( TickType_t ) 0xffffffffUL

#define portCRITICAL_NESTING_IN_TCB 1
#define portSTACK_GROWTH -1
#define configTIMER_QUEUE_LENGTH        2
#define pdFAIL          ( pdFALSE )
#define configTIMER_TASK_PRIORITY		( 2 )
#define configMINIMAL_STACK_SIZE	( ( unsigned short ) 1024 )

#define configTIMER_TASK_STACK_DEPTH	( configMINIMAL_STACK_SIZE )
#define tskIDLE_STACK_SIZE	configMINIMAL_STACK_SIZE
#define portTICK_PERIOD_MS          ( ( TickType_t ) (1000 / configTICK_RATE_HZ) )
#define bktQUEUE_LENGTH             ( 5 )
#define portSTACK_TYPE  uint64_t
#define taskYIELD_IF_USING_PREEMPTION() portYIELD_WITHIN_API()
#define configQUEUE_REGISTRY_SIZE 8
#define portENABLE_INTERRUPTS()                 __asm volatile  ( "csrs mstatus,1" )
#define portDISABLE_INTERRUPTS()                __asm volatile  ( "csrc mstatus,1" )
#endif
