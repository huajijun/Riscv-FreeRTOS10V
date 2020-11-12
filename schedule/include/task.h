#include <common.h>
#include <list.h>
struct tskTaskControlBlock;     /* The old naming convention is used to prevent breaking kernel aware debuggers. */
typedef struct tskTaskControlBlock * TaskHandle_t;


typedef struct xTIME_OUT                                                              
{           
    BaseType_t xOverflowCount;                                                        
    TickType_t xTimeOnEntering;                                                       
} TimeOut_t;
typedef void (*TaskFunction_t)( void * );
            
/*          
 * Defines the memory ranges allocated to the task when an MPU is used.               
 */         
typedef struct xMEMORY_REGION                                                         
{           
    void * pvBaseAddress;                                                             
    uint32_t ulLengthInBytes;                                                         
    uint32_t ulParameters;                                                            
} MemoryRegion_t;                                                                     
#define tskIDLE_PRIORITY    ( ( UBaseType_t ) 0U )
#define taskSCHEDULER_SUSPENDED      ( ( BaseType_t ) 0 )
#define taskSCHEDULER_NOT_STARTED    ( ( BaseType_t ) 1 )
#define taskSCHEDULER_RUNNING        ( ( BaseType_t ) 2 )
void vTaskInternalSetTimeOutState( TimeOut_t * const pxTimeOut );
BaseType_t xTaskCreate( TaskFunction_t pxTaskCode,
                        const char * const pcName, /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
                        const configSTACK_DEPTH_TYPE usStackDepth,
                        void * const pvParameters,
                        UBaseType_t uxPriority,
                        TaskHandle_t * const pxCreatedTask );

void vTaskInternalSetTimeOutState( TimeOut_t * const pxTimeOut );
void vTaskPlaceOnEventList( List_t * const pxEventList,
                            const TickType_t xTicksToWait );
void vTaskStartScheduler( void );
void vTaskSwitchContext( void );
BaseType_t xTaskCheckForTimeOut( TimeOut_t * const pxTimeOut,
                                 TickType_t * const pxTicksToWait );


BaseType_t xTaskGetSchedulerState( void );
TickType_t xTaskGetTickCount( void );

BaseType_t xTaskIncrementTick( void );
BaseType_t xTaskRemoveFromEventList( const List_t * const pxEventList );
void vTaskEnterCritical( void );
void vTaskExitCritical( void );


