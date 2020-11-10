    #define taskSELECT_HIGHEST_PRIORITY_TASK()                                \
    {                                                                         \
        UBaseType_t uxTopPriority = uxTopReadyPriority;                       \
                                                                              \
        /* Find the highest priority queue that contains ready tasks. */      \
        while( listLIST_IS_EMPTY( &( pxReadyTasksLists[ uxTopPriority ] ) ) ) \
        {                                                                     \
            configASSERT( uxTopPriority );                                    \
            --uxTopPriority;                                                  \
        }                                                                     \
                                                                              \
        /* listGET_OWNER_OF_NEXT_ENTRY indexes through the list, so the tasks of \
         * the  same priority get an equal share of the processor time. */                    \
        listGET_OWNER_OF_NEXT_ENTRY( pxCurrentTCB, &( pxReadyTasksLists[ uxTopPriority ] ) ); \
        uxTopReadyPriority = uxTopPriority;                                                   \
    } /* taskSELECT_HIGHEST_PRIORITY_TASK */


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
                                  const char * const pcName, /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
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
            configASSERT( ( ( ( portPOINTER_SIZE_TYPE ) pxTopOfStack & ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) == 0UL ) );

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

            /* Check the alignment of the stack buffer is correct. */
            configASSERT( ( ( ( portPOINTER_SIZE_TYPE ) pxNewTCB->pxStack & ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) == 0UL ) );

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
            else
            {
                mtCOVERAGE_TEST_MARKER();
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
    else
    {
        mtCOVERAGE_TEST_MARKER();
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
    else
    {
        mtCOVERAGE_TEST_MARKER();
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
            else
            {
                mtCOVERAGE_TEST_MARKER();
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
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
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

        portSETUP_TCB( pxNewTCB );
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
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();
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
                else
                {
                    mtCOVERAGE_TEST_MARKER();
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
            else
            {
                mtCOVERAGE_TEST_MARKER();
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

        traceTASK_SWITCHED_IN();

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
        configASSERT( xReturn != errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY );
    }

    /* Prevent compiler warnings if INCLUDE_xTaskGetIdleTaskHandle is set to 0,
     * meaning xIdleTaskHandle is not used anywhere else. */
    ( void ) xIdleTaskHandle;
}