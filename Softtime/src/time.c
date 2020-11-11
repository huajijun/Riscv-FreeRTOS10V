
#include <time.h>
#define portTASK_FUNCTION_PROTO( vFunction, pvParameters )    void vFunction( void * pvParameters )

typedef struct tmrTimerControl                 
{              
    const char * pcTimerName;                  
    ListItem_t xTimerListItem;                 
    TickType_t xTimerPeriodInTicks;            
    void * pvTimerID;                          
    TimerCallbackFunction_t pxCallbackFunction;
    #if ( configUSE_TRACE_FACILITY == 1 )
        UBaseType_t uxTimerNumber;             
    #endif     
    uint8_t ucStatus;                          
} xTIMER;      

typedef xTIMER Timer_t;
typedef struct tmrTimerParameters       
{                                       
    TickType_t xMessageValue; 
    Timer_t * pxTimer;        
} TimerParameter_t;                     
                                        
                                        
typedef struct tmrCallbackParameters    
{                                       
    PendedFunction_t pxCallbackFunction;
    void * pvParameter1;                
    uint32_t ulParameter2;              
} CallbackParameters_t;                 


#define tmrSTATUS_IS_ACTIVE                  ( ( uint8_t ) 0x01 )
#define tmrSTATUS_IS_STATICALLY_ALLOCATED    ( ( uint8_t ) 0x02 )
#define tmrSTATUS_IS_AUTORELOAD              ( ( uint8_t ) 0x04 )


            

static void prvTimerTask( void *pvParameters )                                  
{             
    TickType_t xNextExpireTime;
    BaseType_t xListWasEmpty;
    for( ;; ) 
    {         
        /* Query the timers list to see if it contains any timers, and if so,
        obtain the time at which the next timer will expire. */
        xNextExpireTime = prvGetNextExpireTime( &xListWasEmpty );
              
        /* If a timer has expired, process it.  Otherwise, block this task
        until either a timer does expire, or a command is received. */
        prvProcessTimerOrBlockTask( xNextExpireTime, xListWasEmpty );
              
        /* Empty the command queue. */
        prvProcessReceivedCommands();
    }         
}             
                    



static portTASK_FUNCTION( prvTimerTask, pvParameters )
{
    TickType_t xNextExpireTime;
    BaseType_t xListWasEmpty;

    /* Just to avoid compiler warnings. */
    ( void ) pvParameters;

    #if ( configUSE_DAEMON_TASK_STARTUP_HOOK == 1 )
        {
            extern void vApplicationDaemonTaskStartupHook( void );

            /* Allow the application writer to execute some code in the context of
             * this task at the point the task starts executing.  This is useful if the
             * application includes initialisation code that would benefit from
             * executing after the scheduler has been started. */
            vApplicationDaemonTaskStartupHook();
        }
    #endif /* configUSE_DAEMON_TASK_STARTUP_HOOK */

    for( ; ; )
    {
        /* Query the timers list to see if it contains any timers, and if so,
         * obtain the time at which the next timer will expire. */
        xNextExpireTime = prvGetNextExpireTime( &xListWasEmpty );

        /* If a timer has expired, process it.  Otherwise, block this task
         * until either a timer does expire, or a command is received. */
        prvProcessTimerOrBlockTask( xNextExpireTime, xListWasEmpty );

        /* Empty the command queue. */
        prvProcessReceivedCommands();
    }
}
static void prvCheckForValidListAndQueue( void )
{
    /* Check that the list from which active timers are referenced, and the
     * queue used to communicate with the timer service, have been
     * initialised. */
    taskENTER_CRITICAL();
    {
        if( xTimerQueue == NULL )
        {
            vListInitialise( &xActiveTimerList1 );
            vListInitialise( &xActiveTimerList2 );
            pxCurrentTimerList = &xActiveTimerList1;
            pxOverflowTimerList = &xActiveTimerList2;

            #if ( configSUPPORT_STATIC_ALLOCATION == 1 )
                {
                    /* The timer queue is allocated statically in case
                     * configSUPPORT_DYNAMIC_ALLOCATION is 0. */
                    PRIVILEGED_DATA static StaticQueue_t xStaticTimerQueue;                                                                          /*lint !e956 Ok to declare in this manner to prevent additional conditional compilation guards in other locations. */
                    PRIVILEGED_DATA static uint8_t ucStaticTimerQueueStorage[ ( size_t ) configTIMER_QUEUE_LENGTH * sizeof( DaemonTaskMessage_t ) ]; /*lint !e956 Ok to declare in this manner to prevent additional conditional compilation guards in other locations. */

                    xTimerQueue = xQueueCreateStatic( ( UBaseType_t ) configTIMER_QUEUE_LENGTH, ( UBaseType_t ) sizeof( DaemonTaskMessage_t ), &( ucStaticTimerQueueStorage[ 0 ] ), &xStaticTimerQueue );
                }
            #else
                {
                    xTimerQueue = xQueueCreate( ( UBaseType_t ) configTIMER_QUEUE_LENGTH, sizeof( DaemonTaskMessage_t ) );
                }
            #endif /* if ( configSUPPORT_STATIC_ALLOCATION == 1 ) */

            #if ( configQUEUE_REGISTRY_SIZE > 0 )
                {
                    if( xTimerQueue != NULL )
                    {
                        vQueueAddToRegistry( xTimerQueue, "TmrQ" );
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
            #endif /* configQUEUE_REGISTRY_SIZE */
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }
    }
    taskEXIT_CRITICAL();
}


static TickType_t prvGetNextExpireTime( BaseType_t * const pxListWasEmpty )
{
    TickType_t xNextExpireTime;

    /* Timers are listed in expiry time order, with the head of the list
     * referencing the task that will expire first.  Obtain the time at which
     * the timer with the nearest expiry time will expire.  If there are no
     * active timers then just set the next expire time to 0.  That will cause
     * this task to unblock when the tick count overflows, at which point the
     * timer lists will be switched and the next expiry time can be
     * re-assessed.  */
    *pxListWasEmpty = listLIST_IS_EMPTY( pxCurrentTimerList );

    if( *pxListWasEmpty == pdFALSE )
    {
        xNextExpireTime = listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxCurrentTimerList );
    }
    else
    {
        /* Ensure the task unblocks when the tick count rolls over. */
        xNextExpireTime = ( TickType_t ) 0U;
    }

    return xNextExpireTime;
}

static void prvInitialiseNewTimer( const char * const pcTimerName, /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
                                   const TickType_t xTimerPeriodInTicks,
                                   const UBaseType_t uxAutoReload,
                                   void * const pvTimerID,
                                   TimerCallbackFunction_t pxCallbackFunction,
                                   Timer_t * pxNewTimer )
{

    if( pxNewTimer != NULL )
    {
        /* Ensure the infrastructure used by the timer service task has been
         * created/initialised. */
        prvCheckForValidListAndQueue();

        /* Initialise the timer structure members using the function
         * parameters. */
        pxNewTimer->pcTimerName = pcTimerName;
        pxNewTimer->xTimerPeriodInTicks = xTimerPeriodInTicks;
        pxNewTimer->pvTimerID = pvTimerID;
        pxNewTimer->pxCallbackFunction = pxCallbackFunction;
        vListInitialiseItem( &( pxNewTimer->xTimerListItem ) );

        if( uxAutoReload != pdFALSE )
        {
            pxNewTimer->ucStatus |= tmrSTATUS_IS_AUTORELOAD;
        }

    }
}

static BaseType_t prvInsertTimerInActiveList( Timer_t * const pxTimer,
                                              const TickType_t xNextExpiryTime,
                                              const TickType_t xTimeNow,
                                              const TickType_t xCommandTime )
{
    BaseType_t xProcessTimerNow = pdFALSE;

    listSET_LIST_ITEM_VALUE( &( pxTimer->xTimerListItem ), xNextExpiryTime );
    listSET_LIST_ITEM_OWNER( &( pxTimer->xTimerListItem ), pxTimer );

    if( xNextExpiryTime <= xTimeNow )
    {
        /* Has the expiry time elapsed between the command to start/reset a
         * timer was issued, and the time the command was processed? */
        if( ( ( TickType_t ) ( xTimeNow - xCommandTime ) ) >= pxTimer->xTimerPeriodInTicks ) /*lint !e961 MISRA exception as the casts are only redundant for some ports. */
        {
            /* The time between a command being issued and the command being
             * processed actually exceeds the timers period.  */
            xProcessTimerNow = pdTRUE;
        }
        else
        {
            vListInsert( pxOverflowTimerList, &( pxTimer->xTimerListItem ) );
        }
    }
    else
    {
        if( ( xTimeNow < xCommandTime ) && ( xNextExpiryTime >= xCommandTime ) )
        {
            /* If, since the command was issued, the tick count has overflowed
             * but the expiry time has not, then the timer must have already passed
             * its expiry time and should be processed immediately. */
            xProcessTimerNow = pdTRUE;
        }
        else
        {
            vListInsert( pxCurrentTimerList, &( pxTimer->xTimerListItem ) );
        }
    }

    return xProcessTimerNow;
}


static void prvProcessExpiredTimer( const TickType_t xNextExpireTime,
                                    const TickType_t xTimeNow )
{
    BaseType_t xResult;
    Timer_t * const pxTimer = ( Timer_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxCurrentTimerList ); /*lint !e9087 !e9079 void * is used as this macro is used with tasks and co-routines too.  Alignment is known to be fine as the type of the pointer stored and retrieved is the same. */

    /* Remove the timer from the list of active timers.  A check has already
     * been performed to ensure the list is not empty. */

    ( void ) uxListRemove( &( pxTimer->xTimerListItem ) );
    traceTIMER_EXPIRED( pxTimer );

    /* If the timer is an auto-reload timer then calculate the next
     * expiry time and re-insert the timer in the list of active timers. */
    if( ( pxTimer->ucStatus & tmrSTATUS_IS_AUTORELOAD ) != 0 )
    {
        /* The timer is inserted into a list using a time relative to anything
         * other than the current time.  It will therefore be inserted into the
         * correct list relative to the time this task thinks it is now. */
        if( prvInsertTimerInActiveList( pxTimer, ( xNextExpireTime + pxTimer->xTimerPeriodInTicks ), xTimeNow, xNextExpireTime ) != pdFALSE )
        {
            /* The timer expired before it was added to the active timer
             * list.  Reload it now.  */
            xResult = xTimerGenericCommand( pxTimer, tmrCOMMAND_START_DONT_TRACE, xNextExpireTime, NULL, tmrNO_DELAY );
            ( void ) xResult;
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }
    }
    else
    {
        pxTimer->ucStatus &= ~tmrSTATUS_IS_ACTIVE;
        mtCOVERAGE_TEST_MARKER();
    }

    /* Call the timer callback. */
    pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );
}


static void prvProcessReceivedCommands( void )
{
    DaemonTaskMessage_t xMessage;
    Timer_t * pxTimer;
    BaseType_t xTimerListsWereSwitched, xResult;
    TickType_t xTimeNow;

    while( xQueueReceive( xTimerQueue, &xMessage, tmrNO_DELAY ) != pdFAIL ) /*lint !e603 xMessage does not have to be initialised as it is passed out, not in, and it is not used unless xQueueReceive() returns pdTRUE. */
    {
        #if ( INCLUDE_xTimerPendFunctionCall == 1 )
            {
                /* Negative commands are pended function calls rather than timer
                 * commands. */
                if( xMessage.xMessageID < ( BaseType_t ) 0 )
                {
                    const CallbackParameters_t * const pxCallback = &( xMessage.u.xCallbackParameters );

                    /* The timer uses the xCallbackParameters member to request a
                     * callback be executed.  Check the callback is not NULL. */

                    /* Call the function. */
                    pxCallback->pxCallbackFunction( pxCallback->pvParameter1, pxCallback->ulParameter2 );
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }
            }
        #endif /* INCLUDE_xTimerPendFunctionCall */

        /* Commands that are positive are timer commands rather than pended
         * function calls. */
        if( xMessage.xMessageID >= ( BaseType_t ) 0 )
        {
            /* The messages uses the xTimerParameters member to work on a
             * software timer. */
            pxTimer = xMessage.u.xTimerParameters.pxTimer;

            if( listIS_CONTAINED_WITHIN( NULL, &( pxTimer->xTimerListItem ) ) == pdFALSE ) /*lint !e961. The cast is only redundant when NULL is passed into the macro. */
            {
                /* The timer is in a list, remove it. */
                ( void ) uxListRemove( &( pxTimer->xTimerListItem ) );
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }

            traceTIMER_COMMAND_RECEIVED( pxTimer, xMessage.xMessageID, xMessage.u.xTimerParameters.xMessageValue );

            /* In this case the xTimerListsWereSwitched parameter is not used, but
             *  it must be present in the function call.  prvSampleTimeNow() must be
             *  called after the message is received from xTimerQueue so there is no
             *  possibility of a higher priority task adding a message to the message
             *  queue with a time that is ahead of the timer daemon task (because it
             *  pre-empted the timer daemon task after the xTimeNow value was set). */
            xTimeNow = prvSampleTimeNow( &xTimerListsWereSwitched );

            switch( xMessage.xMessageID )
            {
                case tmrCOMMAND_START:
                case tmrCOMMAND_START_FROM_ISR:
                case tmrCOMMAND_RESET:
                case tmrCOMMAND_RESET_FROM_ISR:
                case tmrCOMMAND_START_DONT_TRACE:
                    /* Start or restart a timer. */
                    pxTimer->ucStatus |= tmrSTATUS_IS_ACTIVE;

                    if( prvInsertTimerInActiveList( pxTimer, xMessage.u.xTimerParameters.xMessageValue + pxTimer->xTimerPeriodInTicks, xTimeNow, xMessage.u.xTimerParameters.xMessageValue ) != pdFALSE )
                    {
                        /* The timer expired before it was added to the active
                         * timer list.  Process it now. */
                        pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );
                        traceTIMER_EXPIRED( pxTimer );

                        if( ( pxTimer->ucStatus & tmrSTATUS_IS_AUTORELOAD ) != 0 )
                        {
                            xResult = xTimerGenericCommand( pxTimer, tmrCOMMAND_START_DONT_TRACE, xMessage.u.xTimerParameters.xMessageValue + pxTimer->xTimerPeriodInTicks, NULL, tmrNO_DELAY );
                            ( void ) xResult;
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }

                    break;

                case tmrCOMMAND_STOP:
                case tmrCOMMAND_STOP_FROM_ISR:
                    /* The timer has already been removed from the active list. */
                    pxTimer->ucStatus &= ~tmrSTATUS_IS_ACTIVE;
                    break;

                case tmrCOMMAND_CHANGE_PERIOD:
                case tmrCOMMAND_CHANGE_PERIOD_FROM_ISR:
                    pxTimer->ucStatus |= tmrSTATUS_IS_ACTIVE;
                    pxTimer->xTimerPeriodInTicks = xMessage.u.xTimerParameters.xMessageValue;

                    /* The new period does not really have a reference, and can
                     * be longer or shorter than the old one.  The command time is
                     * therefore set to the current time, and as the period cannot
                     * be zero the next expiry time can only be in the future,
                     * meaning (unlike for the xTimerStart() case above) there is
                     * no fail case that needs to be handled here. */
                    ( void ) prvInsertTimerInActiveList( pxTimer, ( xTimeNow + pxTimer->xTimerPeriodInTicks ), xTimeNow, xTimeNow );
                    break;

                case tmrCOMMAND_DELETE:
                    #if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
                        {
                            /* The timer has already been removed from the active list,
                             * just free up the memory if the memory was dynamically
                             * allocated. */
                            if( ( pxTimer->ucStatus & tmrSTATUS_IS_STATICALLY_ALLOCATED ) == ( uint8_t ) 0 )
                            {
                                vPortFree( pxTimer );
                            }
                            else
                            {
                                pxTimer->ucStatus &= ~tmrSTATUS_IS_ACTIVE;
                            }
                        }
                    #else /* if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) */
                        {
                            /* If dynamic allocation is not enabled, the memory
                             * could not have been dynamically allocated. So there is
                             * no need to free the memory - just mark the timer as
                             * "not active". */
                            pxTimer->ucStatus &= ~tmrSTATUS_IS_ACTIVE;
                        }
                    #endif /* configSUPPORT_DYNAMIC_ALLOCATION */
                    break;

                default:
                    /* Don't expect to get here. */
                    break;
            }
        }
    }
}


static void prvProcessTimerOrBlockTask( const TickType_t xNextExpireTime,
                                        BaseType_t xListWasEmpty )
{
    TickType_t xTimeNow;
    BaseType_t xTimerListsWereSwitched;

    vTaskSuspendAll();
    {
        /* Obtain the time now to make an assessment as to whether the timer
         * has expired or not.  If obtaining the time causes the lists to switch
         * then don't process this timer as any timers that remained in the list
         * when the lists were switched will have been processed within the
         * prvSampleTimeNow() function. */
        xTimeNow = prvSampleTimeNow( &xTimerListsWereSwitched );

        if( xTimerListsWereSwitched == pdFALSE )
        {
            /* The tick count has not overflowed, has the timer expired? */
            if( ( xListWasEmpty == pdFALSE ) && ( xNextExpireTime <= xTimeNow ) )
            {
                ( void ) xTaskResumeAll();
                prvProcessExpiredTimer( xNextExpireTime, xTimeNow );
            }
            else
            {
                /* The tick count has not overflowed, and the next expire
                 * time has not been reached yet.  This task should therefore
                 * block to wait for the next expire time or a command to be
                 * received - whichever comes first.  The following line cannot
                 * be reached unless xNextExpireTime > xTimeNow, except in the
                 * case when the current timer list is empty. */
                if( xListWasEmpty != pdFALSE )
                {
                    /* The current timer list is empty - is the overflow list
                     * also empty? */
                    xListWasEmpty = listLIST_IS_EMPTY( pxOverflowTimerList );
                }

                vQueueWaitForMessageRestricted( xTimerQueue, ( xNextExpireTime - xTimeNow ), xListWasEmpty );

                if( xTaskResumeAll() == pdFALSE )
                {
                    /* Yield to wait for either a command to arrive, or the
                     * block time to expire.  If a command arrived between the
                     * critical section being exited and this yield then the yield
                     * will not cause the task to block. */
                    portYIELD_WITHIN_API();
                }
            }
        }
        else
        {
            ( void ) xTaskResumeAll();
        }
    }
}

static TickType_t prvSampleTimeNow( BaseType_t * const pxTimerListsWereSwitched )
{
    TickType_t xTimeNow;
    PRIVILEGED_DATA static TickType_t xLastTime = ( TickType_t ) 0U; /*lint !e956 Variable is only accessible to one task. */

    xTimeNow = xTaskGetTickCount();

    if( xTimeNow < xLastTime )
    {
        prvSwitchTimerLists();
        *pxTimerListsWereSwitched = pdTRUE;
    }
    else
    {
        *pxTimerListsWereSwitched = pdFALSE;
    }

    xLastTime = xTimeNow;

    return xTimeNow;
}

static void prvSwitchTimerLists( void )
{
    TickType_t xNextExpireTime, xReloadTime;
    List_t * pxTemp;
    Timer_t * pxTimer;
    BaseType_t xResult;

    /* The tick count has overflowed.  The timer lists must be switched.
     * If there are any timers still referenced from the current timer list
     * then they must have expired and should be processed before the lists
     * are switched. */
    while( listLIST_IS_EMPTY( pxCurrentTimerList ) == pdFALSE )
    {
        xNextExpireTime = listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxCurrentTimerList );

        /* Remove the timer from the list. */
        pxTimer = ( Timer_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxCurrentTimerList ); /*lint !e9087 !e9079 void * is used as this macro is used with tasks and co-routines too.  Alignment is known to be fine as the type of the pointer stored and retrieved is the same. */
        ( void ) uxListRemove( &( pxTimer->xTimerListItem ) );

        /* Execute its callback, then send a command to restart the timer if
         * it is an auto-reload timer.  It cannot be restarted here as the lists
         * have not yet been switched. */
        pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );

        if( ( pxTimer->ucStatus & tmrSTATUS_IS_AUTORELOAD ) != 0 )
        {
            /* Calculate the reload value, and if the reload value results in
             * the timer going into the same timer list then it has already expired
             * and the timer should be re-inserted into the current list so it is
             * processed again within this loop.  Otherwise a command should be sent
             * to restart the timer to ensure it is only inserted into a list after
             * the lists have been swapped. */
            xReloadTime = ( xNextExpireTime + pxTimer->xTimerPeriodInTicks );

            if( xReloadTime > xNextExpireTime )
            {
                listSET_LIST_ITEM_VALUE( &( pxTimer->xTimerListItem ), xReloadTime );
                listSET_LIST_ITEM_OWNER( &( pxTimer->xTimerListItem ), pxTimer );
                vListInsert( pxCurrentTimerList, &( pxTimer->xTimerListItem ) );
            }
            else
            {
                xResult = xTimerGenericCommand( pxTimer, tmrCOMMAND_START_DONT_TRACE, xNextExpireTime, NULL, tmrNO_DELAY );
                ( void ) xResult;
            }
        }
    }

    pxTemp = pxCurrentTimerList;
    pxCurrentTimerList = pxOverflowTimerList;
    pxOverflowTimerList = pxTemp;
}

TimerHandle_t xTimerCreate( const char * const pcTimerName, /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
                            const TickType_t xTimerPeriodInTicks,
                            const UBaseType_t uxAutoReload,
                            void * const pvTimerID,
                            TimerCallbackFunction_t pxCallbackFunction )
{
    Timer_t * pxNewTimer;

    pxNewTimer = ( Timer_t * ) pvPortMalloc( sizeof( Timer_t ) ); /*lint !e9087 !e9079 All values returned by pvPortMalloc() have at least the alignment required by the MCU's stack, and the first member of Timer_t is always a pointer to the timer's mame. */

    if( pxNewTimer != NULL )
    {
        /* Status is thus far zero as the timer is not created statically
         * and has not been started.  The auto-reload bit may get set in
         * prvInitialiseNewTimer. */
        pxNewTimer->ucStatus = 0x00;
        prvInitialiseNewTimer( pcTimerName, xTimerPeriodInTicks, uxAutoReload, pvTimerID, pxCallbackFunction, pxNewTimer );
    }

    return pxNewTimer;
}

BaseType_t xTimerCreateTimerTask( void )
{
    BaseType_t xReturn = pdFAIL;

    /* This function is called when the scheduler is started if
     * configUSE_TIMERS is set to 1.  Check that the infrastructure used by the
     * timer service task has been created/initialised.  If timers have already
     * been created then the initialisation will already have been performed. */
    prvCheckForValidListAndQueue();

    if( xTimerQueue != NULL )
    {
        #if ( configSUPPORT_STATIC_ALLOCATION == 1 )
            {
                StaticTask_t * pxTimerTaskTCBBuffer = NULL;
                StackType_t * pxTimerTaskStackBuffer = NULL;
                uint32_t ulTimerTaskStackSize;

                vApplicationGetTimerTaskMemory( &pxTimerTaskTCBBuffer, &pxTimerTaskStackBuffer, &ulTimerTaskStackSize );
                xTimerTaskHandle = xTaskCreateStatic( prvTimerTask,
                                                      configTIMER_SERVICE_TASK_NAME,
                                                      ulTimerTaskStackSize,
                                                      NULL,
                                                      ( ( UBaseType_t ) configTIMER_TASK_PRIORITY ) | portPRIVILEGE_BIT,
                                                      pxTimerTaskStackBuffer,
                                                      pxTimerTaskTCBBuffer );

                if( xTimerTaskHandle != NULL )
                {
                    xReturn = pdPASS;
                }
            }
        #else /* if ( configSUPPORT_STATIC_ALLOCATION == 1 ) */
            {
                xReturn = xTaskCreate( prvTimerTask,
                                       configTIMER_SERVICE_TASK_NAME,
                                       configTIMER_TASK_STACK_DEPTH,
                                       NULL,
                                       ( ( UBaseType_t ) configTIMER_TASK_PRIORITY ) | portPRIVILEGE_BIT,
                                       &xTimerTaskHandle );
            }
        #endif /* configSUPPORT_STATIC_ALLOCATION */
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();
    }

    return xReturn;
}
    BaseType_t xTimerGenericCommand( TimerHandle_t xTimer,
                                 const BaseType_t xCommandID,
                                 const TickType_t xOptionalValue,
                                 BaseType_t * const pxHigherPriorityTaskWoken,
                                 const TickType_t xTicksToWait )
{
    BaseType_t xReturn = pdFAIL;
    DaemonTaskMessage_t xMessage;


    /* Send a message to the timer service task to perform a particular action
     * on a particular timer definition. */
    if( xTimerQueue != NULL )
    {
        /* Send a command to the timer service task to start the xTimer timer. */
        xMessage.xMessageID = xCommandID;
        xMessage.u.xTimerParameters.xMessageValue = xOptionalValue;
        xMessage.u.xTimerParameters.pxTimer = xTimer;

        if( xCommandID < tmrFIRST_FROM_ISR_COMMAND )
        {
            if( xTaskGetSchedulerState() == taskSCHEDULER_RUNNING )
            {
                xReturn = xQueueSendToBack( xTimerQueue, &xMessage, xTicksToWait );
            }
            else
            {
                xReturn = xQueueSendToBack( xTimerQueue, &xMessage, tmrNO_DELAY );
            }
        }
        else
        {
            xReturn = xQueueSendToBackFromISR( xTimerQueue, &xMessage, pxHigherPriorityTaskWoken );
        }

    }

    return xReturn;
}

TickType_t xTimerGetExpiryTime( TimerHandle_t xTimer )
{
    Timer_t * pxTimer = xTimer;
    TickType_t xReturn;

    xReturn = listGET_LIST_ITEM_VALUE( &( pxTimer->xTimerListItem ) );
    return xReturn;
}


static void prvSetNextTimerInterrupt(void)
{
    __asm volatile("ld t0,0(%0)"::"r"mtimecmp);
    __asm volatile("add t0,t0,%0" :: "r"(configTICK_CLOCK_HZ / configTICK_RATE_HZ));
    __asm volatile("sd t0,0(%0)"::"r"mtimecmp);
}




void vPortSetupTimer(void)
{
  __asm volatile("ld t0,0(%0)"::"r"mtime);
  __asm volatile("add t0,t0,%0"::"r"(configTICK_CLOCK_HZ / configTICK_RATE_HZ));
  __asm volatile("sd t0,0(%0)"::"r"mtimecmp);

   /* Enable timer interupt */
   __asm volatile("csrs mie,%0"::"r"(0x80));
}

void vPortSysTickHandler( void )
{
    prvSetNextTimerInterrupt();

    /* Increment the RTOS tick. */
    if( xTaskIncrementTick() != pdFALSE )
    {
        vTaskSwitchContext();
    }
}
static void prvCheckForValidListAndQueue( void )
{
  if( xTimerQueue == NULL )
  {
      vListInitialise( &xActiveTimerList1 );
      vListInitialise( &xActiveTimerList2 );
      pxCurrentTimerList = &xActiveTimerList1;
      pxOverflowTimerList = &xActiveTimerList2;
      xTimerQueue = xQueueCreate( ( UBaseType_t ) configTIMER_QUEUE_LENGTH, sizeof( DaemonTaskMessage_t ) );
      {
          if( xTimerQueue != NULL )
          {
              vQueueAddToRegistry( xTimerQueue, "TmrQ" );
          }
      }
  }
}

