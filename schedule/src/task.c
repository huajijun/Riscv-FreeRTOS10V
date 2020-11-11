#include <task.h>
#include <list.h>
#include <common.h>
typedef struct tskTaskControlBlock       /* The old naming convention is used to prevent breaking kernel aware debuggers. */
{
    volatile StackType_t * pxTopOfStack; /*< Points to the location of the last item placed on the tasks stack.  THIS MUST BE THE FIRST MEMBER OF THE TCB STRUCT. */

    #if ( portUSING_MPU_WRAPPERS == 1 )
        xMPU_SETTINGS xMPUSettings; /*< The MPU settings are defined as part of the port layer.  THIS MUST BE THE SECOND MEMBER OF THE TCB STRUCT. */
    #endif

    ListItem_t xStateListItem;                  /*< The list that the state list item of a task is reference from denotes the state of that task (Ready, Blocked, Suspended ). */
    ListItem_t xEventListItem;                  /*< Used to reference a task from an event list. */
    UBaseType_t uxPriority;                     /*< The priority of the task.  0 is the lowest priority. */
    StackType_t * pxStack;                      /*< Points to the start of the stack. */
    char pcTaskName[ configMAX_TASK_NAME_LEN ]; /*< Descriptive name given to the task when created.  Facilitates debugging only. */ /*lint !e971 Unqualified char types are allowed for strings and single characters only. */

    #if ( ( portSTACK_GROWTH > 0 ) || ( configRECORD_STACK_HIGH_ADDRESS == 1 ) )
        StackType_t * pxEndOfStack; /*< Points to the highest valid address for the stack. */
    #endif

    #if ( portCRITICAL_NESTING_IN_TCB == 1 )
        UBaseType_t uxCriticalNesting; /*< Holds the critical section nesting depth for ports that do not maintain their own count in the port layer. */
    #endif

    #if ( configUSE_TRACE_FACILITY == 1 )
        UBaseType_t uxTCBNumber;  /*< Stores a number that increments each time a TCB is created.  It allows debuggers to determine when a task has been deleted and then recreated. */
        UBaseType_t uxTaskNumber; /*< Stores a number specifically for use by third party trace code. */
    #endif

    #if ( configUSE_MUTEXES == 1 )
        UBaseType_t uxBasePriority; /*< The priority last assigned to the task - used by the priority inheritance mechanism. */
        UBaseType_t uxMutexesHeld;
    #endif

    #if ( configUSE_APPLICATION_TASK_TAG == 1 )
        TaskHookFunction_t pxTaskTag;
    #endif

    #if ( configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0 )
        void * pvThreadLocalStoragePointers[ configNUM_THREAD_LOCAL_STORAGE_POINTERS ];
    #endif

    #if ( configGENERATE_RUN_TIME_STATS == 1 )
        uint32_t ulRunTimeCounter; /*< Stores the amount of time the task has spent in the Running state. */
    #endif

    #if ( configUSE_NEWLIB_REENTRANT == 1 )

        /* Allocate a Newlib reent structure that is specific to this task.
         * Note Newlib support has been included by popular demand, but is not
         * used by the FreeRTOS maintainers themselves.  FreeRTOS is not
         * responsible for resulting newlib operation.  User must be familiar with
         * newlib and must provide system-wide implementations of the necessary
         * stubs. Be warned that (at the time of writing) the current newlib design
         * implements a system-wide malloc() that must be provided with locks.
         *
         * See the third party link http://www.nadler.com/embedded/newlibAndFreeRTOS.html
         * for additional information. */
        struct  _reent xNewLib_reent;
    #endif

    #if ( configUSE_TASK_NOTIFICATIONS == 1 )
        volatile uint32_t ulNotifiedValue[ configTASK_NOTIFICATION_ARRAY_ENTRIES ];
        volatile uint8_t ucNotifyState[ configTASK_NOTIFICATION_ARRAY_ENTRIES ];
    #endif

    /* See the comments in FreeRTOS.h with the definition of
     * tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE. */
    #if ( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 ) /*lint !e731 !e9029 Macro has been consolidated for readability reasons. */
        uint8_t ucStaticallyAllocated;                     /*< Set to pdTRUE if the task is a statically allocated to ensure no attempt is made to free the memory. */
    #endif

    #if ( INCLUDE_xTaskAbortDelay == 1 )
        uint8_t ucDelayAborted;
    #endif

    #if ( configUSE_POSIX_ERRNO == 1 )
        int iTaskErrno;
    #endif
} tskTCB;

/* The old tskTCB name is maintained above then typedefed to the new TCB_t name
 * below to enable the use of older kernel aware debuggers. */
typedef tskTCB TCB_t;
#define portBYTE_ALIGNMENT_MASK (0x001f)                                
static volatile BaseType_t xSchedulerRunning        = pdFALSE;          
static volatile UBaseType_t uxTopReadyPriority      = tskIDLE_PRIORITY; 
static List_t pxReadyTasksLists[ configMAX_PRIORITIES ];                
static List_t xDelayedTaskList1;                                        
static List_t xDelayedTaskList2;                                        
static List_t * volatile pxDelayedTaskList;                             
static List_t * volatile pxOverflowDelayedTaskList;                     
static List_t xPendingReadyList;       
static volatile UBaseType_t uxCurrentNumberOfTasks  = ( UBaseType_t ) 0U;
static volatile TickType_t xTickCount               = ( TickType_t ) 0U; 
                                       
static List_t xSuspendedTaskList;      
                                       
static volatile UBaseType_t uxPendedTicks           = ( UBaseType_t ) 0U;
static volatile BaseType_t xYieldPending            = pdFALSE;           
static volatile BaseType_t xNumOfOverflows          = ( BaseType_t ) 0;  
static UBaseType_t uxTaskNumber                     = ( UBaseType_t ) 0U;
static volatile TickType_t xNextTaskUnblockTime     = ( TickType_t ) 0U; 
TCB_t * volatile pxCurrentTCB = NULL;                                    



    #define taskSELECT_HIGHEST_PRIORITY_TASK()                                \
    {                                                                         \
        UBaseType_t uxTopPriority = uxTopReadyPriority;                       \
                                                                              \
        /* Find the highest priority queue that contains ready tasks. */      \
        while( listLIST_IS_EMPTY( &( pxReadyTasksLists[ uxTopPriority ] ) ) ) \
        {                                                                     \
            --uxTopPriority;                                                  \
        }                                                                     \
                                                                              \
        /* listGET_OWNER_OF_NEXT_ENTRY indexes through the list, so the tasks of \
         * the  same priority get an equal share of the processor time. */                    \
        listGET_OWNER_OF_NEXT_ENTRY( pxCurrentTCB, &( pxReadyTasksLists[ uxTopPriority ] ) ); \
        uxTopReadyPriority = uxTopPriority;                                                   \
    } /* taskSELECT_HIGHEST_PRIORITY_TASK */

 #define taskRECORD_READY_PRIORITY( uxPriority ) \
    {                                               \
        if( ( uxPriority ) > uxTopReadyPriority )   \
        {                                           \
            uxTopReadyPriority = ( uxPriority );    \
        }                                           \
    } /* taskRECORD_READY_PRIORITY */


    #define prvAddTaskToReadyList( pxTCB )                                                                 \
    taskRECORD_READY_PRIORITY( ( pxTCB )->uxPriority );                                                \
    vListInsertEnd( &( pxReadyTasksLists[ ( pxTCB )->uxPriority ] ), &( ( pxTCB )->xStateListItem ) ); \


static void prvInitialiseTaskLists( void );
static void prvInitialiseTaskLists( void );
static void prvAddCurrentTaskToDelayedList( TickType_t xTicksToWait,
                                            const BaseType_t xCanBlockIndefinitely );
static void prvAddNewTaskToReadyList( TCB_t * pxNewTCB );
static void prvInitialiseNewTask( TaskFunction_t pxTaskCode,
                                  const char * const pcName, 
                                  const uint32_t ulStackDepth,
                                  void * const pvParameters,
                                  UBaseType_t uxPriority,
                                  TaskHandle_t * const pxCreatedTask,
                                  TCB_t * pxNewTCB,
                                  const MemoryRegion_t * const xRegions );


void vTaskInternalSetTimeOutState( TimeOut_t * const pxTimeOut )
{
    /* For internal use only as it does not use a critical section. */
    pxTimeOut->xOverflowCount = xNumOfOverflows;
    pxTimeOut->xTimeOnEntering = xTickCount;
}

BaseType_t xTaskCreate( TaskFunction_t pxTaskCode,
                        const char * const pcName, /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
                        const configSTACK_DEPTH_TYPE usStackDepth,
                        void * const pvParameters,
                        UBaseType_t uxPriority,
                        TaskHandle_t * const pxCreatedTask )
{
    TCB_t * pxNewTCB;
    BaseType_t xReturn;

    /* If the stack grows down then allocate the stack then the TCB so the stack
     * does not grow into the TCB.  Likewise if the stack grows up then allocate
     * the TCB then the stack. */
    #if ( portSTACK_GROWTH > 0 )
        {
            /* Allocate space for the TCB.  Where the memory comes from depends on
             * the implementation of the port malloc function and whether or not static
             * allocation is being used. */
            pxNewTCB = ( TCB_t * ) pvPortMalloc( sizeof( TCB_t ) );

            if( pxNewTCB != NULL )
            {
                /* Allocate space for the stack used by the task being created.
                 * The base of the stack memory stored in the TCB so the task can
                 * be deleted later if required. */
                pxNewTCB->pxStack = ( StackType_t * ) pvPortMalloc( ( ( ( size_t ) usStackDepth ) * sizeof( StackType_t ) ) ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */

                if( pxNewTCB->pxStack == NULL )
                {
                    /* Could not allocate the stack.  Delete the allocated TCB. */
                    vPortFree( pxNewTCB );
                    pxNewTCB = NULL;
                }
            }
        }
    #else /* portSTACK_GROWTH */
        {
            StackType_t * pxStack;

            /* Allocate space for the stack used by the task being created. */
            pxStack = pvPortMalloc( ( ( ( size_t ) usStackDepth ) * sizeof( StackType_t ) ) ); /*lint !e9079 All values returned by pvPortMalloc() have at least the alignment required by the MCU's stack and this allocation is the stack. */

            if( pxStack != NULL )
            {
                /* Allocate space for the TCB. */
                pxNewTCB = ( TCB_t * ) pvPortMalloc( sizeof( TCB_t ) ); /*lint !e9087 !e9079 All values returned by pvPortMalloc() have at least the alignment required by the MCU's stack, and the first member of TCB_t is always a pointer to the task's stack. */

                if( pxNewTCB != NULL )
                {
                    /* Store the stack location in the TCB. */
                    pxNewTCB->pxStack = pxStack;
                }
                else
                {
                    /* The stack cannot be used as the TCB was not created.  Free
                     * it again. */
                    vPortFree( pxStack );
                }
            }
            else
            {
                pxNewTCB = NULL;
            }
        }
    #endif /* portSTACK_GROWTH */

    if( pxNewTCB != NULL )
    {
        #if ( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 ) /*lint !e9029 !e731 Macro has been consolidated for readability reasons. */
            {
                /* Tasks can be created statically or dynamically, so note this
                 * task was created dynamically in case it is later deleted. */
                pxNewTCB->ucStaticallyAllocated = tskDYNAMICALLY_ALLOCATED_STACK_AND_TCB;
            }
        #endif /* tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE */

        prvInitialiseNewTask( pxTaskCode, pcName, ( uint32_t ) usStackDepth, pvParameters, uxPriority, pxCreatedTask, pxNewTCB, NULL );
        prvAddNewTaskToReadyList( pxNewTCB );
        xReturn = pdPASS;
    }
    else
    {
        xReturn = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }

    return xReturn;
}

static void prvInitialiseNewTask( TaskFunction_t pxTaskCode,
                                  const char * const pcName, 
                                  const uint32_t ulStackDepth,
                                  void * const pvParameters,
                                  UBaseType_t uxPriority,
                                  TaskHandle_t * const pxCreatedTask,
                                  TCB_t * pxNewTCB,
                                  const MemoryRegion_t * const xRegions )
{
    StackType_t * pxTopOfStack;
    UBaseType_t x;

    #if ( portUSING_MPU_WRAPPERS == 1 )
        /* Should the task be created in privileged mode? */
        BaseType_t xRunPrivileged;

        if( ( uxPriority & portPRIVILEGE_BIT ) != 0U )
        {
            xRunPrivileged = pdTRUE;
        }
        else
        {
            xRunPrivileged = pdFALSE;
        }
        uxPriority &= ~portPRIVILEGE_BIT;
    #endif /* portUSING_MPU_WRAPPERS == 1 */

    /* Avoid dependency on memset() if it is not required. */
    #if ( tskSET_NEW_STACKS_TO_KNOWN_VALUE == 1 )
        {
            /* Fill the stack with a known value to assist debugging. */
            ( void ) memset( pxNewTCB->pxStack, ( int ) tskSTACK_FILL_BYTE, ( size_t ) ulStackDepth * sizeof( StackType_t ) );
        }
    #endif /* tskSET_NEW_STACKS_TO_KNOWN_VALUE */

    /* Calculate the top of stack address.  This depends on whether the stack
     * grows from high memory to low (as per the 80x86) or vice versa.
     * portSTACK_GROWTH is used to make the result positive or negative as required
     * by the port. */
    #if ( portSTACK_GROWTH < 0 )
        {
            pxTopOfStack = &( pxNewTCB->pxStack[ ulStackDepth - ( uint32_t ) 1 ] );
            pxTopOfStack = ( StackType_t * ) ( ( ( portPOINTER_SIZE_TYPE ) pxTopOfStack ) & ( ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) ) ); /*lint !e923 !e9033 !e9078 MISRA exception.  Avoiding casts between pointers and integers is not practical.  Size differences accounted for using portPOINTER_SIZE_TYPE type.  Checked by assert(). */

            /* Check the alignment of the calculated top of stack is correct. */

            #if ( configRECORD_STACK_HIGH_ADDRESS == 1 )
                {
                    /* Also record the stack's high address, which may assist
                     * debugging. */
                    pxNewTCB->pxEndOfStack = pxTopOfStack;
                }
            #endif /* configRECORD_STACK_HIGH_ADDRESS */
        }
    #else /* portSTACK_GROWTH */
        {
            pxTopOfStack = pxNewTCB->pxStack;


            /* The other extreme of the stack space is required if stack checking is
             * performed. */
            pxNewTCB->pxEndOfStack = pxNewTCB->pxStack + ( ulStackDepth - ( uint32_t ) 1 );
        }
    #endif /* portSTACK_GROWTH */

    /* Store the task name in the TCB. */
    if( pcName != NULL )
    {
        for( x = ( UBaseType_t ) 0; x < ( UBaseType_t ) configMAX_TASK_NAME_LEN; x++ )
        {
            pxNewTCB->pcTaskName[ x ] = pcName[ x ];

            /* Don't copy all configMAX_TASK_NAME_LEN if the string is shorter than
             * configMAX_TASK_NAME_LEN characters just in case the memory after the
             * string is not accessible (extremely unlikely). */
            if( pcName[ x ] == ( char ) 0x00 )
            {
                break;
            }
        }

        /* Ensure the name string is terminated in the case that the string length
         * was greater or equal to configMAX_TASK_NAME_LEN. */
        pxNewTCB->pcTaskName[ configMAX_TASK_NAME_LEN - 1 ] = '\0';
    }
    else
    {
        /* The task has not been given a name, so just ensure there is a NULL
         * terminator when it is read out. */
        pxNewTCB->pcTaskName[ 0 ] = 0x00;
    }

    /* This is used as an array index so must ensure it's not too large.  First
     * remove the privilege bit if one is present. */
    if( uxPriority >= ( UBaseType_t ) configMAX_PRIORITIES )
    {
        uxPriority = ( UBaseType_t ) configMAX_PRIORITIES - ( UBaseType_t ) 1U;
    }

    pxNewTCB->uxPriority = uxPriority;
    #if ( configUSE_MUTEXES == 1 )
        {
            pxNewTCB->uxBasePriority = uxPriority;
            pxNewTCB->uxMutexesHeld = 0;
        }
    #endif /* configUSE_MUTEXES */

    vListInitialiseItem( &( pxNewTCB->xStateListItem ) );
    vListInitialiseItem( &( pxNewTCB->xEventListItem ) );

    /* Set the pxNewTCB as a link back from the ListItem_t.  This is so we can get
     * back to  the containing TCB from a generic item in a list. */
    listSET_LIST_ITEM_OWNER( &( pxNewTCB->xStateListItem ), pxNewTCB );

    /* Event lists are always in priority order. */
    listSET_LIST_ITEM_VALUE( &( pxNewTCB->xEventListItem ), ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) uxPriority ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */
    listSET_LIST_ITEM_OWNER( &( pxNewTCB->xEventListItem ), pxNewTCB );

    #if ( portCRITICAL_NESTING_IN_TCB == 1 )
        {
            pxNewTCB->uxCriticalNesting = ( UBaseType_t ) 0U;
        }
    #endif /* portCRITICAL_NESTING_IN_TCB */

    #if ( configUSE_APPLICATION_TASK_TAG == 1 )
        {
            pxNewTCB->pxTaskTag = NULL;
        }
    #endif /* configUSE_APPLICATION_TASK_TAG */

    #if ( configGENERATE_RUN_TIME_STATS == 1 )
        {
            pxNewTCB->ulRunTimeCounter = 0UL;
        }
    #endif /* configGENERATE_RUN_TIME_STATS */

    #if ( portUSING_MPU_WRAPPERS == 1 )
        {
            vPortStoreTaskMPUSettings( &( pxNewTCB->xMPUSettings ), xRegions, pxNewTCB->pxStack, ulStackDepth );
        }
    #else
        {
            /* Avoid compiler warning about unreferenced parameter. */
            ( void ) xRegions;
        }
    #endif

    #if ( configNUM_THREAD_LOCAL_STORAGE_POINTERS != 0 )
        {
            memset( ( void * ) &( pxNewTCB->pvThreadLocalStoragePointers[ 0 ] ), 0x00, sizeof( pxNewTCB->pvThreadLocalStoragePointers ) );
        }
    #endif

    #if ( configUSE_TASK_NOTIFICATIONS == 1 )
        {
            memset( ( void * ) &( pxNewTCB->ulNotifiedValue[ 0 ] ), 0x00, sizeof( pxNewTCB->ulNotifiedValue ) );
            memset( ( void * ) &( pxNewTCB->ucNotifyState[ 0 ] ), 0x00, sizeof( pxNewTCB->ucNotifyState ) );
        }
    #endif

    #if ( configUSE_NEWLIB_REENTRANT == 1 )
        {
            /* Initialise this task's Newlib reent structure.
             * See the third party link http://www.nadler.com/embedded/newlibAndFreeRTOS.html
             * for additional information. */
            _REENT_INIT_PTR( ( &( pxNewTCB->xNewLib_reent ) ) );
        }
    #endif

    #if ( INCLUDE_xTaskAbortDelay == 1 )
        {
            pxNewTCB->ucDelayAborted = pdFALSE;
        }
    #endif

    /* Initialize the TCB stack to look as if the task was already running,
     * but had been interrupted by the scheduler.  The return address is set
     * to the start of the task function. Once the stack has been initialised
     * the top of stack variable is updated. */
    #if ( portUSING_MPU_WRAPPERS == 1 )
        {
            /* If the port has capability to detect stack overflow,
             * pass the stack end address to the stack initialization
             * function as well. */
            #if ( portHAS_STACK_OVERFLOW_CHECKING == 1 )
                {
                    #if ( portSTACK_GROWTH < 0 )
                        {
                            pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack, pxNewTCB->pxStack, pxTaskCode, pvParameters, xRunPrivileged );
                        }
                    #else /* portSTACK_GROWTH */
                        {
                            pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack, pxNewTCB->pxEndOfStack, pxTaskCode, pvParameters, xRunPrivileged );
                        }
                    #endif /* portSTACK_GROWTH */
                }
            #else /* portHAS_STACK_OVERFLOW_CHECKING */
                {
                    pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack, pxTaskCode, pvParameters, xRunPrivileged );
                }
            #endif /* portHAS_STACK_OVERFLOW_CHECKING */
        }
    #else /* portUSING_MPU_WRAPPERS */
        {
            /* If the port has capability to detect stack overflow,
             * pass the stack end address to the stack initialization
             * function as well. */
            #if ( portHAS_STACK_OVERFLOW_CHECKING == 1 )
                {
                    #if ( portSTACK_GROWTH < 0 )
                        {
                            pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack, pxNewTCB->pxStack, pxTaskCode, pvParameters );
                        }
                    #else /* portSTACK_GROWTH */
                        {
                            pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack, pxNewTCB->pxEndOfStack, pxTaskCode, pvParameters );
                        }
                    #endif /* portSTACK_GROWTH */
                }
            #else /* portHAS_STACK_OVERFLOW_CHECKING */
                {
                    pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack, pxTaskCode, pvParameters );
                }
            #endif /* portHAS_STACK_OVERFLOW_CHECKING */
        }
    #endif /* portUSING_MPU_WRAPPERS */

    if( pxCreatedTask != NULL )
    {
        /* Pass the handle out in an anonymous way.  The handle can be used to
         * change the created task's priority, delete the created task, etc.*/
        *pxCreatedTask = ( TaskHandle_t ) pxNewTCB;
    }
}

static void prvAddNewTaskToReadyList( TCB_t * pxNewTCB )
{
    /* Ensure interrupts don't access the task lists while the lists are being
     * updated. */
    taskENTER_CRITICAL();
    {
        uxCurrentNumberOfTasks++;

        if( pxCurrentTCB == NULL )
        {
            /* There are no other tasks, or all the other tasks are in
             * the suspended state - make this the current task. */
            pxCurrentTCB = pxNewTCB;

            if( uxCurrentNumberOfTasks == ( UBaseType_t ) 1 )
            {
                /* This is the first task to be created so do the preliminary
                 * initialisation required.  We will not recover if this call
                 * fails, but we will report the failure. */
                prvInitialiseTaskLists();
            }
        }
        else
        {
            /* If the scheduler is not already running, make this task the
             * current task if it is the highest priority task to be created
             * so far. */
            if( xSchedulerRunning == pdFALSE )
            {
                if( pxCurrentTCB->uxPriority <= pxNewTCB->uxPriority )
                {
                    pxCurrentTCB = pxNewTCB;
                }
            }
        }

        uxTaskNumber++;

        #if ( configUSE_TRACE_FACILITY == 1 )
            {
                /* Add a counter into the TCB for tracing only. */
                pxNewTCB->uxTCBNumber = uxTaskNumber;
            }
        #endif /* configUSE_TRACE_FACILITY */
        traceTASK_CREATE( pxNewTCB );

        prvAddTaskToReadyList( pxNewTCB );

    }
    taskEXIT_CRITICAL();

    if( xSchedulerRunning != pdFALSE )
    {
        /* If the created task is of a higher priority than the current task
         * then it should run now. */
        if( pxCurrentTCB->uxPriority < pxNewTCB->uxPriority )
        {
            taskYIELD_IF_USING_PREEMPTION();
        }
    }
}

static void prvInitialiseTaskLists( void )
{
    UBaseType_t uxPriority;

    for( uxPriority = ( UBaseType_t ) 0U; uxPriority < ( UBaseType_t ) configMAX_PRIORITIES; uxPriority++ )
    {
        vListInitialise( &( pxReadyTasksLists[ uxPriority ] ) );
    }

    vListInitialise( &xDelayedTaskList1 );
    vListInitialise( &xDelayedTaskList2 );
    vListInitialise( &xPendingReadyList );

    #if ( INCLUDE_vTaskDelete == 1 )
        {
            vListInitialise( &xTasksWaitingTermination );
        }
    #endif /* INCLUDE_vTaskDelete */

    #if ( INCLUDE_vTaskSuspend == 1 )
        {
            vListInitialise( &xSuspendedTaskList );
        }
    #endif /* INCLUDE_vTaskSuspend */

    /* Start with pxDelayedTaskList using list1 and the pxOverflowDelayedTaskList
     * using list2. */
    pxDelayedTaskList = &xDelayedTaskList1;
    pxOverflowDelayedTaskList = &xDelayedTaskList2;
}


void vTaskSwitchContext( void )
{
    if( uxSchedulerSuspended != ( UBaseType_t ) pdFALSE )
    {
        /* The scheduler is currently suspended - do not allow a context
         * switch. */
        xYieldPending = pdTRUE;
    }
    else
    {
        xYieldPending = pdFALSE;
        traceTASK_SWITCHED_OUT();

        #if ( configGENERATE_RUN_TIME_STATS == 1 )
            {
                #ifdef portALT_GET_RUN_TIME_COUNTER_VALUE
                    portALT_GET_RUN_TIME_COUNTER_VALUE( ulTotalRunTime );
                #else
                    ulTotalRunTime = portGET_RUN_TIME_COUNTER_VALUE();
                #endif

                /* Add the amount of time the task has been running to the
                 * accumulated time so far.  The time the task started running was
                 * stored in ulTaskSwitchedInTime.  Note that there is no overflow
                 * protection here so count values are only valid until the timer
                 * overflows.  The guard against negative values is to protect
                 * against suspect run time stat counter implementations - which
                 * are provided by the application, not the kernel. */
                if( ulTotalRunTime > ulTaskSwitchedInTime )
                {
                    pxCurrentTCB->ulRunTimeCounter += ( ulTotalRunTime - ulTaskSwitchedInTime );
                }

                ulTaskSwitchedInTime = ulTotalRunTime;
            }
        #endif /* configGENERATE_RUN_TIME_STATS */

        /* Check for stack overflow, if configured. */
        taskCHECK_FOR_STACK_OVERFLOW();

        /* Before the currently running task is switched out, save its errno. */
        #if ( configUSE_POSIX_ERRNO == 1 )
            {
                pxCurrentTCB->iTaskErrno = FreeRTOS_errno;
            }
        #endif

        /* Select a new task to run using either the generic C or port
         * optimised asm code. */
        taskSELECT_HIGHEST_PRIORITY_TASK(); /*lint !e9079 void * is used as this macro is used with timers and co-routines too.  Alignment is known to be fine as the type of the pointer stored and retrieved is the same. */
        traceTASK_SWITCHED_IN();

        /* After the new task is switched in, update the global errno. */
        #if ( configUSE_POSIX_ERRNO == 1 )
            {
                FreeRTOS_errno = pxCurrentTCB->iTaskErrno;
            }
        #endif

        #if ( configUSE_NEWLIB_REENTRANT == 1 )
            {
                /* Switch Newlib's _impure_ptr variable to point to the _reent
                 * structure specific to this task.
                 * See the third party link http://www.nadler.com/embedded/newlibAndFreeRTOS.html
                 * for additional information. */
                _impure_ptr = &( pxCurrentTCB->xNewLib_reent );
            }
        #endif /* configUSE_NEWLIB_REENTRANT */
    }
}

void vTaskStartScheduler( void )
{
    BaseType_t xReturn;

    /* Add the idle task at the lowest priority. */
    #if ( configSUPPORT_STATIC_ALLOCATION == 1 )
        {
            StaticTask_t * pxIdleTaskTCBBuffer = NULL;
            StackType_t * pxIdleTaskStackBuffer = NULL;
            uint32_t ulIdleTaskStackSize;

            /* The Idle task is created using user provided RAM - obtain the
             * address of the RAM then create the idle task. */
            vApplicationGetIdleTaskMemory( &pxIdleTaskTCBBuffer, &pxIdleTaskStackBuffer, &ulIdleTaskStackSize );
            xIdleTaskHandle = xTaskCreateStatic( prvIdleTask,
                                                 configIDLE_TASK_NAME,
                                                 ulIdleTaskStackSize,
                                                 ( void * ) NULL,       /*lint !e961.  The cast is not redundant for all compilers. */
                                                 portPRIVILEGE_BIT,     /* In effect ( tskIDLE_PRIORITY | portPRIVILEGE_BIT ), but tskIDLE_PRIORITY is zero. */
                                                 pxIdleTaskStackBuffer,
                                                 pxIdleTaskTCBBuffer ); /*lint !e961 MISRA exception, justified as it is not a redundant explicit cast to all supported compilers. */

            if( xIdleTaskHandle != NULL )
            {
                xReturn = pdPASS;
            }
            else
            {
                xReturn = pdFAIL;
            }
        }
    #else /* if ( configSUPPORT_STATIC_ALLOCATION == 1 ) */
        {
            /* The Idle task is being created using dynamically allocated RAM. */
            xReturn = xTaskCreate( prvIdleTask,
                                   configIDLE_TASK_NAME,
                                   configMINIMAL_STACK_SIZE,
                                   ( void * ) NULL,
                                   portPRIVILEGE_BIT,  /* In effect ( tskIDLE_PRIORITY | portPRIVILEGE_BIT ), but tskIDLE_PRIORITY is zero. */
                                   &xIdleTaskHandle ); /*lint !e961 MISRA exception, justified as it is not a redundant explicit cast to all supported compilers. */
        }
    #endif /* configSUPPORT_STATIC_ALLOCATION */

    #if ( configUSE_TIMERS == 1 )
        {
            if( xReturn == pdPASS )
            {
                xReturn = xTimerCreateTimerTask();
            }
        }
    #endif /* configUSE_TIMERS */

    if( xReturn == pdPASS )
    {
        /* freertos_tasks_c_additions_init() should only be called if the user
         * definable macro FREERTOS_TASKS_C_ADDITIONS_INIT() is defined, as that is
         * the only macro called by the function. */
        #ifdef FREERTOS_TASKS_C_ADDITIONS_INIT
            {
                freertos_tasks_c_additions_init();
            }
        #endif

        /* Interrupts are turned off here, to ensure a tick does not occur
         * before or during the call to xPortStartScheduler().  The stacks of
         * the created tasks contain a status word with interrupts switched on
         * so interrupts will automatically get re-enabled when the first task
         * starts to run. */
        portDISABLE_INTERRUPTS();

        #if ( configUSE_NEWLIB_REENTRANT == 1 )
            {
                /* Switch Newlib's _impure_ptr variable to point to the _reent
                 * structure specific to the task that will run first.
                 * See the third party link http://www.nadler.com/embedded/newlibAndFreeRTOS.html
                 * for additional information. */
                _impure_ptr = &( pxCurrentTCB->xNewLib_reent );
            }
        #endif /* configUSE_NEWLIB_REENTRANT */

        xNextTaskUnblockTime = portMAX_DELAY;
        xSchedulerRunning = pdTRUE;
        xTickCount = ( TickType_t ) configINITIAL_TICK_COUNT;

        /* If configGENERATE_RUN_TIME_STATS is defined then the following
         * macro must be defined to configure the timer/counter used to generate
         * the run time counter time base.   NOTE:  If configGENERATE_RUN_TIME_STATS
         * is set to 0 and the following line fails to build then ensure you do not
         * have portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() defined in your
         * FreeRTOSConfig.h file. */
        portCONFIGURE_TIMER_FOR_RUN_TIME_STATS();


        /* Setting up the timer tick is hardware specific and thus in the
         * portable interface. */
        if( xPortStartScheduler() != pdFALSE )
        {
            /* Should not reach here as if the scheduler is running the
             * function will not return. */
        }
        else
        {
            /* Should only reach here if a task calls xTaskEndScheduler(). */
        }
    }
    else
    {
        /* This line will only be reached if the kernel could not be started,
         * because there was not enough FreeRTOS heap to create the idle task
         * or the timer task. */
    }

    /* Prevent compiler warnings if INCLUDE_xTaskGetIdleTaskHandle is set to 0,
     * meaning xIdleTaskHandle is not used anywhere else. */
    ( void ) xIdleTaskHandle;
}

BaseType_t xTaskRemoveFromEventList( const List_t * const pxEventList )
{
    TCB_t * pxUnblockedTCB;
    BaseType_t xReturn;

    /* THIS FUNCTION MUST BE CALLED FROM A CRITICAL SECTION.  It can also be
     * called from a critical section within an ISR. */

    /* The event list is sorted in priority order, so the first in the list can
     * be removed as it is known to be the highest priority.  Remove the TCB from
     * the delayed list, and add it to the ready list.
     *
     * If an event is for a queue that is locked then this function will never
     * get called - the lock count on the queue will get modified instead.  This
     * means exclusive access to the event list is guaranteed here.
     *
     * This function assumes that a check has already been made to ensure that
     * pxEventList is not empty. */
    pxUnblockedTCB = listGET_OWNER_OF_HEAD_ENTRY( pxEventList ); /*lint !e9079 void * is used as this macro is used with timers and co-routines too.  Alignment is known to be fine as the type of the pointer stored and retrieved is the same. */
    ( void ) uxListRemove( &( pxUnblockedTCB->xEventListItem ) );

    if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
    {
        ( void ) uxListRemove( &( pxUnblockedTCB->xStateListItem ) );
        prvAddTaskToReadyList( pxUnblockedTCB );

        #if ( configUSE_TICKLESS_IDLE != 0 )
            {
                /* If a task is blocked on a kernel object then xNextTaskUnblockTime
                 * might be set to the blocked task's time out time.  If the task is
                 * unblocked for a reason other than a timeout xNextTaskUnblockTime is
                 * normally left unchanged, because it is automatically reset to a new
                 * value when the tick count equals xNextTaskUnblockTime.  However if
                 * tickless idling is used it might be more important to enter sleep mode
                 * at the earliest possible time - so reset xNextTaskUnblockTime here to
                 * ensure it is updated at the earliest possible time. */
                prvResetNextTaskUnblockTime();
            }
        #endif
    }
    else
    {
        /* The delayed and ready lists cannot be accessed, so hold this task
         * pending until the scheduler is resumed. */
        vListInsertEnd( &( xPendingReadyList ), &( pxUnblockedTCB->xEventListItem ) );
    }

    if( pxUnblockedTCB->uxPriority > pxCurrentTCB->uxPriority )
    {
        /* Return true if the task removed from the event list has a higher
         * priority than the calling task.  This allows the calling task to know if
         * it should force a context switch now. */
        xReturn = pdTRUE;

        /* Mark that a yield is pending in case the user is not using the
         * "xHigherPriorityTaskWoken" parameter to an ISR safe FreeRTOS function. */
        xYieldPending = pdTRUE;
    }
    else
    {
        xReturn = pdFALSE;
    }

    return xReturn;
}

BaseType_t xTaskCheckForTimeOut( TimeOut_t * const pxTimeOut,
                                 TickType_t * const pxTicksToWait )
{
    BaseType_t xReturn;


    taskENTER_CRITICAL();
    {
        /* Minor optimisation.  The tick count cannot change in this block. */
        const TickType_t xConstTickCount = xTickCount;
        const TickType_t xElapsedTime = xConstTickCount - pxTimeOut->xTimeOnEntering;

        #if ( INCLUDE_xTaskAbortDelay == 1 )
            if( pxCurrentTCB->ucDelayAborted != ( uint8_t ) pdFALSE )
            {
                /* The delay was aborted, which is not the same as a time out,
                 * but has the same result. */
                pxCurrentTCB->ucDelayAborted = pdFALSE;
                xReturn = pdTRUE;
            }
            else
        #endif

        #if ( INCLUDE_vTaskSuspend == 1 )
            if( *pxTicksToWait == portMAX_DELAY )
            {
                /* If INCLUDE_vTaskSuspend is set to 1 and the block time
                 * specified is the maximum block time then the task should block
                 * indefinitely, and therefore never time out. */
                xReturn = pdFALSE;
            }
            else
        #endif

        if( ( xNumOfOverflows != pxTimeOut->xOverflowCount ) && ( xConstTickCount >= pxTimeOut->xTimeOnEntering ) ) /*lint !e525 Indentation preferred as is to make code within pre-processor directives clearer. */
        {
            /* The tick count is greater than the time at which
             * vTaskSetTimeout() was called, but has also overflowed since
             * vTaskSetTimeOut() was called.  It must have wrapped all the way
             * around and gone past again. This passed since vTaskSetTimeout()
             * was called. */
            xReturn = pdTRUE;
            *pxTicksToWait = ( TickType_t ) 0;
        }
        else if( xElapsedTime < *pxTicksToWait ) /*lint !e961 Explicit casting is only redundant with some compilers, whereas others require it to prevent integer conversion errors. */
        {
            /* Not a genuine timeout. Adjust parameters for time remaining. */
            *pxTicksToWait -= xElapsedTime;
            vTaskInternalSetTimeOutState( pxTimeOut );
            xReturn = pdFALSE;
        }
        else
        {
            *pxTicksToWait = ( TickType_t ) 0;
            xReturn = pdTRUE;
        }
    }
    taskEXIT_CRITICAL();

    return xReturn;
}


void vTaskPlaceOnEventList( List_t * const pxEventList,
                            const TickType_t xTicksToWait )
{

    /* THIS FUNCTION MUST BE CALLED WITH EITHER INTERRUPTS DISABLED OR THE
     * SCHEDULER SUSPENDED AND THE QUEUE BEING ACCESSED LOCKED. */

    /* Place the event list item of the TCB in the appropriate event list.
     * This is placed in the list in priority order so the highest priority task
     * is the first to be woken by the event.  The queue that contains the event
     * list is locked, preventing simultaneous access from interrupts. */
    vListInsert( pxEventList, &( pxCurrentTCB->xEventListItem ) );

    prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );
}



static void prvAddCurrentTaskToDelayedList( TickType_t xTicksToWait,
                                            const BaseType_t xCanBlockIndefinitely )
{
    TickType_t xTimeToWake;
    const TickType_t xConstTickCount = xTickCount;

    #if ( INCLUDE_xTaskAbortDelay == 1 )
        {
            /* About to enter a delayed list, so ensure the ucDelayAborted flag is
             * reset to pdFALSE so it can be detected as having been set to pdTRUE
             * when the task leaves the Blocked state. */
            pxCurrentTCB->ucDelayAborted = pdFALSE;
        }
    #endif

    /* Remove the task from the ready list before adding it to the blocked list
     * as the same list item is used for both lists. */
    if( uxListRemove( &( pxCurrentTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
    {
        /* The current task must be in a ready list, so there is no need to
         * check, and the port reset macro can be called directly. */
        portRESET_READY_PRIORITY( pxCurrentTCB->uxPriority, uxTopReadyPriority ); /*lint !e931 pxCurrentTCB cannot change as it is the calling task.  pxCurrentTCB->uxPriority and uxTopReadyPriority cannot change as called with scheduler suspended or in a critical section. */
    }

    #if ( INCLUDE_vTaskSuspend == 1 )
        {
            if( ( xTicksToWait == portMAX_DELAY ) && ( xCanBlockIndefinitely != pdFALSE ) )
            {
                /* Add the task to the suspended task list instead of a delayed task
                 * list to ensure it is not woken by a timing event.  It will block
                 * indefinitely. */
                vListInsertEnd( &xSuspendedTaskList, &( pxCurrentTCB->xStateListItem ) );
            }
            else
            {
                /* Calculate the time at which the task should be woken if the event
                 * does not occur.  This may overflow but this doesn't matter, the
                 * kernel will manage it correctly. */
                xTimeToWake = xConstTickCount + xTicksToWait;

                /* The list item will be inserted in wake time order. */
                listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xStateListItem ), xTimeToWake );

                if( xTimeToWake < xConstTickCount )
                {
                    /* Wake time has overflowed.  Place this item in the overflow
                     * list. */
                    vListInsert( pxOverflowDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );
                }
                else
                {
                    /* The wake time has not overflowed, so the current block list
                     * is used. */
                    vListInsert( pxDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );

                    /* If the task entering the blocked state was placed at the
                     * head of the list of blocked tasks then xNextTaskUnblockTime
                     * needs to be updated too. */
                    if( xTimeToWake < xNextTaskUnblockTime )
                    {
                        xNextTaskUnblockTime = xTimeToWake;
                    }
                }
            }
        }
    #else /* INCLUDE_vTaskSuspend */
        {
            /* Calculate the time at which the task should be woken if the event
             * does not occur.  This may overflow but this doesn't matter, the kernel
             * will manage it correctly. */
            xTimeToWake = xConstTickCount + xTicksToWait;

            /* The list item will be inserted in wake time order. */
            listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xStateListItem ), xTimeToWake );

            if( xTimeToWake < xConstTickCount )
            {
                /* Wake time has overflowed.  Place this item in the overflow list. */
                vListInsert( pxOverflowDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );
            }
            else
            {
                /* The wake time has not overflowed, so the current block list is used. */
                vListInsert( pxDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );

                /* If the task entering the blocked state was placed at the head of the
                 * list of blocked tasks then xNextTaskUnblockTime needs to be updated
                 * too. */
                if( xTimeToWake < xNextTaskUnblockTime )
                {
                    xNextTaskUnblockTime = xTimeToWake;
                }
            }

            /* Avoid compiler warning when INCLUDE_vTaskSuspend is not 1. */
        }
    #endif /* INCLUDE_vTaskSuspend */
}


BaseType_t xTaskIncrementTick( void )
{
    TCB_t * pxTCB;
    TickType_t xItemValue;
    BaseType_t xSwitchRequired = pdFALSE;

    /* Called by the portable layer each time a tick interrupt occurs.
     * Increments the tick then checks to see if the new tick value will cause any
     * tasks to be unblocked. */
    traceTASK_INCREMENT_TICK( xTickCount );

    if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
    {
        /* Minor optimisation.  The tick count cannot change in this
         * block. */
        const TickType_t xConstTickCount = xTickCount + ( TickType_t ) 1;

        /* Increment the RTOS tick, switching the delayed and overflowed
         * delayed lists if it wraps to 0. */
        xTickCount = xConstTickCount;

        if( xConstTickCount == ( TickType_t ) 0U ) /*lint !e774 'if' does not always evaluate to false as it is looking for an overflow. */
        {
            taskSWITCH_DELAYED_LISTS();
        }

        /* See if this tick has made a timeout expire.  Tasks are stored in
         * the  queue in the order of their wake time - meaning once one task
         * has been found whose block time has not expired there is no need to
         * look any further down the list. */
        if( xConstTickCount >= xNextTaskUnblockTime )
        {
            for( ; ; )
            {
                if( listLIST_IS_EMPTY( pxDelayedTaskList ) != pdFALSE )
                {
                    /* The delayed list is empty.  Set xNextTaskUnblockTime
                     * to the maximum possible value so it is extremely
                     * unlikely that the
                     * if( xTickCount >= xNextTaskUnblockTime ) test will pass
                     * next time through. */
                    xNextTaskUnblockTime = portMAX_DELAY; /*lint !e961 MISRA exception as the casts are only redundant for some ports. */
                    break;
                }
                else
                {
                    /* The delayed list is not empty, get the value of the
                     * item at the head of the delayed list.  This is the time
                     * at which the task at the head of the delayed list must
                     * be removed from the Blocked state. */
                    pxTCB = listGET_OWNER_OF_HEAD_ENTRY( pxDelayedTaskList ); /*lint !e9079 void * is used as this macro is used with timers and co-routines too.  Alignment is known to be fine as the type of the pointer stored and retrieved is the same. */
                    xItemValue = listGET_LIST_ITEM_VALUE( &( pxTCB->xStateListItem ) );

                    if( xConstTickCount < xItemValue )
                    {
                        /* It is not time to unblock this item yet, but the
                         * item value is the time at which the task at the head
                         * of the blocked list must be removed from the Blocked
                         * state -  so record the item value in
                         * xNextTaskUnblockTime. */
                        xNextTaskUnblockTime = xItemValue;
                        break; /*lint !e9011 Code structure here is deedmed easier to understand with multiple breaks. */
                    }

                    /* It is time to remove the item from the Blocked state. */
                    ( void ) uxListRemove( &( pxTCB->xStateListItem ) );

                    /* Is the task waiting on an event also?  If so remove
                     * it from the event list. */
                    if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
                    {
                        ( void ) uxListRemove( &( pxTCB->xEventListItem ) );
                    }

                    /* Place the unblocked task into the appropriate ready
                     * list. */
                    prvAddTaskToReadyList( pxTCB );

                    /* A task being unblocked cannot cause an immediate
                     * context switch if preemption is turned off. */
                    #if ( configUSE_PREEMPTION == 1 )
                        {
                            /* Preemption is on, but a context switch should
                             * only be performed if the unblocked task has a
                             * priority that is equal to or higher than the
                             * currently executing task. */
                            if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority )
                            {
                                xSwitchRequired = pdTRUE;
                            }
                        }
                    #endif /* configUSE_PREEMPTION */
                }
            }
        }

        /* Tasks of equal priority to the currently running task will share
         * processing time (time slice) if preemption is on, and the application
         * writer has not explicitly turned time slicing off. */
        #if ( ( configUSE_PREEMPTION == 1 ) && ( configUSE_TIME_SLICING == 1 ) )
            {
                if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ pxCurrentTCB->uxPriority ] ) ) > ( UBaseType_t ) 1 )
                {
                    xSwitchRequired = pdTRUE;
                }
            }
        #endif /* ( ( configUSE_PREEMPTION == 1 ) && ( configUSE_TIME_SLICING == 1 ) ) */

        #if ( configUSE_TICK_HOOK == 1 )
            {
                /* Guard against the tick hook being called when the pended tick
                 * count is being unwound (when the scheduler is being unlocked). */
                if( xPendedTicks == ( TickType_t ) 0 )
                {
                    vApplicationTickHook();
                }
            }
        #endif /* configUSE_TICK_HOOK */

        #if ( configUSE_PREEMPTION == 1 )
            {
                if( xYieldPending != pdFALSE )
                {
                    xSwitchRequired = pdTRUE;
                }
            }
        #endif /* configUSE_PREEMPTION */
    }
    else
    {
        ++xPendedTicks;

        /* The tick hook gets called at regular intervals, even if the
         * scheduler is locked. */
        #if ( configUSE_TICK_HOOK == 1 )
            {
                vApplicationTickHook();
            }
        #endif
    }

    return xSwitchRequired;
}

TickType_t xTaskGetTickCount( void )
{
    TickType_t xTicks;
 
    /* Critical section required if running on a 16 bit processor. */
    portTICK_TYPE_ENTER_CRITICAL();
    {
        xTicks = xTickCount;
    }
    portTICK_TYPE_EXIT_CRITICAL();
 
    return xTicks;
}               

BaseType_t xTaskGetSchedulerState( void )
{
    BaseType_t xReturn;

    if( xSchedulerRunning == pdFALSE )
    {
        xReturn = taskSCHEDULER_NOT_STARTED;
    }
    else     
    {
        if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
        {    
            xReturn = taskSCHEDULER_RUNNING;
        }    
        else 
        {    
            xReturn = taskSCHEDULER_SUSPENDED;
        }    
    }

    return xReturn;
}                                                               

static portTASK_FUNCTION( prvIdleTask, pvParameters )
{
    for(;;)
    {
        if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ tskIDLE_PRIORITY ] ) ) > ( UBaseType_t ) 1 )
        {
            taskYIELD();
        }
    }
}                                                                                                       

void vTaskEnterCritical( void )
	{
		portDISABLE_INTERRUPTS();

		if( xSchedulerRunning != pdFALSE )
		{
			( pxCurrentTCB->uxCriticalNesting )++;

			/* This is not the interrupt safe version of the enter critical
			function so	assert() if it is being called from an interrupt
			context.  Only API functions that end in "FromISR" can be used in an
			interrupt.  Only assert if the critical nesting count is 1 to
			protect against recursive calls if the assert function also uses a
			critical section. */
			if( pxCurrentTCB->uxCriticalNesting == 1 )
			{
				//portASSERT_IF_IN_ISR();
			}
		}
		else
		{
			
		}
	}
	void vTaskExitCritical( void )
	{
		if( xSchedulerRunning != pdFALSE )
		{
			if( pxCurrentTCB->uxCriticalNesting > 0U )
			{
				( pxCurrentTCB->uxCriticalNesting )--;

				if( pxCurrentTCB->uxCriticalNesting == 0U )
				{
					portENABLE_INTERRUPTS();
				}
			}
		}
			
	}

BaseType_t xTaskResumeAll( void )          
{                                          
	TCB_t *pxTCB;                              
	BaseType_t xAlreadyYielded = pdFALSE;
	taskENTER_CRITICAL();                                                                     
	{                                                                                         
	    --uxSchedulerSuspended;
	                                                                                          
	    if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
	    {
	        if( uxCurrentNumberOfTasks > ( UBaseType_t ) 0U )
	        {
	            /* Move any readied tasks from the pending list into the
	            appropriate ready list. */
	            while( listLIST_IS_EMPTY( &xPendingReadyList ) == pdFALSE )
	            {
	                pxTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( ( &xPendingReadyList ) );
	                ( void ) uxListRemove( &( pxTCB->xEventListItem ) );
	                ( void ) uxListRemove( &( pxTCB->xGenericListItem ) );
	                prvAddTaskToReadyList( pxTCB );
	                                                                                          
	                /* If the moved task has a priority higher than the current
	                task then a yield must be performed. */
	                if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority )
	                {
	                    xYieldPending = pdTRUE;
	                }
	            }
	                                                                                          
	            if( uxPendedTicks > ( UBaseType_t ) 0U )
	            {
	                while( uxPendedTicks > ( UBaseType_t ) 0U )
	                {
	                    if( xTaskIncrementTick() != pdFALSE )
	                    {
	                        xYieldPending = pdTRUE;
	                    }
	                    --uxPendedTicks;
	                }
	            }                                              
	            if( xYieldPending == pdTRUE )                        
	            {   
	                #if( configUSE_PREEMPTION != 0 )                 
	                {                                                
	                    xAlreadyYielded = pdTRUE;                     
	                }                                                
	                #endif                                           
	                taskYIELD_IF_USING_PREEMPTION();                 
	            }   
 
	        }       
	    }           
        
	}               
	taskEXIT_CRITICAL();                                             
	                
	return xAlreadyYielded;                                          
}
void vTaskSuspendAll( void )
 {
 	++uxSchedulerSuspended;
 }
  void vTaskPlaceOnEventListRestricted( List_t * const pxEventList,
                                          TickType_t xTicksToWait,
                                          const BaseType_t xWaitIndefinitely )
    {
        configASSERT( pxEventList );

        /* This function should not be called by application code hence the
         * 'Restricted' in its name.  It is not part of the public API.  It is
         * designed for use by kernel code, and has special calling requirements -
         * it should be called with the scheduler suspended. */


        /* Place the event list item of the TCB in the appropriate event list.
         * In this case it is assume that this is the only task that is going to
         * be waiting on this event list, so the faster vListInsertEnd() function
         * can be used in place of vListInsert. */
        vListInsertEnd( pxEventList, &( pxCurrentTCB->xEventListItem ) );

        /* If the task should block indefinitely then set the block time to a
         * value that will be recognised as an indefinite delay inside the
         * prvAddCurrentTaskToDelayedList() function. */
        if( xWaitIndefinitely != pdFALSE )
        {
            xTicksToWait = portMAX_DELAY;
        }

        traceTASK_DELAY_UNTIL( ( xTickCount + xTicksToWait ) );
        prvAddCurrentTaskToDelayedList( xTicksToWait, xWaitIndefinitely );
    }
