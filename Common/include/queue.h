#include <common.h>
#ifndef QUEUE_H
#define QUEUE_H

#define queueYIELD_IF_USING_PREEMPTION() portYIELD_WITHIN_API()
struct QueueDefinition; /* Using old naming convention so as not to break kernel aware debuggers. */
typedef struct QueueDefinition   * QueueHandle_t;

typedef struct QueueDefinition   * QueueSetHandle_t;

typedef struct QueueDefinition   * QueueSetMemberHandle_t;

#define taskENTER_CRITICAL()		portENTER_CRITICAL()
#define taskEXIT_CRITICAL()			portEXIT_CRITICAL()

#define portENTER_CRITICAL()					vTaskEnterCritical()


#define portEXIT_CRITICAL()						vTaskExitCritical()


#define queueSEND_TO_BACK                     ( ( BaseType_t ) 0 )
#define queueSEND_TO_FRONT                    ( ( BaseType_t ) 1 )
#define queueOVERWRITE                        ( ( BaseType_t ) 2 )

/* For internal use only.  These definitions *must* match those in queue.c. */
#define queueQUEUE_TYPE_BASE                  ( ( uint8_t ) 0U )
#define queueQUEUE_TYPE_SET                   ( ( uint8_t ) 0U )
#define queueQUEUE_TYPE_MUTEX                 ( ( uint8_t ) 1U )
#define queueQUEUE_TYPE_COUNTING_SEMAPHORE    ( ( uint8_t ) 2U )
#define queueQUEUE_TYPE_BINARY_SEMAPHORE      ( ( uint8_t ) 3U )
#define queueQUEUE_TYPE_RECURSIVE_MUTEX       ( ( uint8_t ) 4U )

 #define xQueueCreate( uxQueueLength, uxItemSize )    xQueueGenericCreate( ( uxQueueLength ), ( uxItemSize ), ( queueQUEUE_TYPE_BASE ) )


#define xQueueSendToFront( xQueue, pvItemToQueue, xTicksToWait ) \
    xQueueGenericSend( ( xQueue ), ( pvItemToQueue ), ( xTicksToWait ), queueSEND_TO_FRONT )


#define xQueueSendToBack( xQueue, pvItemToQueue, xTicksToWait ) \
    xQueueGenericSend( ( xQueue ), ( pvItemToQueue ), ( xTicksToWait ), queueSEND_TO_BACK )


#define xQueueSend( xQueue, pvItemToQueue, xTicksToWait ) \
    xQueueGenericSend( ( xQueue ), ( pvItemToQueue ), ( xTicksToWait ), queueSEND_TO_BACK )

#define xQueueOverwrite( xQueue, pvItemToQueue ) \
    xQueueGenericSend( ( xQueue ), ( pvItemToQueue ), 0, queueOVERWRITE )

BaseType_t xQueueGenericSend( QueueHandle_t xQueue,const void * const pvItemToQueue,TickType_t xTicksToWait,const BaseType_t xCopyPosition );

BaseType_t xQueuePeek( QueueHandle_t xQueue,void * const pvBuffer,TickType_t xTicksToWait );

BaseType_t xQueuePeekFromISR( QueueHandle_t xQueue,void * const pvBuffer );

BaseType_t xQueueReceive( QueueHandle_t xQueue,void * const pvBuffer,TickType_t xTicksToWait );

UBaseType_t uxQueueMessagesWaiting( const QueueHandle_t xQueue ) ;

void vQueueDelete( QueueHandle_t xQueue ) ;


#define xQueueSendToFrontFromISR( xQueue, pvItemToQueue, pxHigherPriorityTaskWoken ) \
    xQueueGenericSendFromISR( ( xQueue ), ( pvItemToQueue ), ( pxHigherPriorityTaskWoken ), queueSEND_TO_FRONT )

#define xQueueSendToBackFromISR( xQueue, pvItemToQueue, pxHigherPriorityTaskWoken ) \
    xQueueGenericSendFromISR( ( xQueue ), ( pvItemToQueue ), ( pxHigherPriorityTaskWoken ), queueSEND_TO_BACK )

#define xQueueSendFromISR( xQueue, pvItemToQueue, pxHigherPriorityTaskWoken ) \
    xQueueGenericSendFromISR( ( xQueue ), ( pvItemToQueue ), ( pxHigherPriorityTaskWoken ), queueSEND_TO_BACK )

BaseType_t xQueueGenericSendFromISR( QueueHandle_t xQueue,const void * const pvItemToQueue,BaseType_t * const pxHigherPriorityTaskWoken,const BaseType_t xCopyPosition );
BaseType_t xQueueGiveFromISR( QueueHandle_t xQueue,BaseType_t * const pxHigherPriorityTaskWoken );


void vQueueAddToRegistry( QueueHandle_t xQueue,const char * pcQueueName );


QueueHandle_t xQueueGenericCreate( const UBaseType_t uxQueueLength,const UBaseType_t uxItemSize,const uint8_t ucQueueType );


BaseType_t xQueueGenericSend( QueueHandle_t xQueue,const void * const pvItemToQueue,TickType_t xTicksToWait,const BaseType_t xCopyPosition );


BaseType_t xQueueReceive( QueueHandle_t xQueue,void * const pvBuffer,TickType_t xTicksToWait);
    

#endif
