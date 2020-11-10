#include <heap2.h>
#include <queue2.h>
#include <common.h>
#include <task.h>
#include <time.h>

#define mainCHECK_TIMER_PERIOD_MS           ( 3000UL / portTICK_PERIOD_MS )
#define mainDONT_BLOCK                      ( 0UL )	
BaseType_t xData = 0;
void TestProgram(void);
void prvCheckTimerCallback(void);
void vSyscallInit(void)
{
	TimerHandle_t xCheckTimer = NULL;

	xTaskCreate( TestProgram, "TestProgram", 4096, NULL, 20, NULL );

	xCheckTimer = xTimerCreate( "CheckTimer",( mainCHECK_TIMER_PERIOD_MS ),pdTRUE,( void * ) 0,prvCheckTimerCallback);    

	if( xCheckTimer != NULL )                      
	{           
	    xTimerStart( xCheckTimer, mainDONT_BLOCK );
	}           
                          

	vTaskStartScheduler();
}

void TestProgram(void)
{

	BaseType_t a =1;
	QueueHandle_t testqueuet;
	testqueuet = xQueueCreate( bktQUEUE_LENGTH, sizeof( BaseType_t ) );
	xQueueSend(testqueuet,&a,0);
	if( xQueueReceive( testqueuet, &xData, 0 ) != errQUEUE_EMPTY )
	{
		//xErrorOccurred = pdTRUE;
	}
}
void prvCheckTimerCallback(void)
{
	int a=0;
}
