#include <common.h>
typedef struct xTIME_OUT                                                              
{           
    BaseType_t xOverflowCount;                                                        
    TickType_t xTimeOnEntering;                                                       
} TimeOut_t;
            
/*          
 * Defines the memory ranges allocated to the task when an MPU is used.               
 */         
typedef struct xMEMORY_REGION                                                         
{           
    void * pvBaseAddress;                                                             
    uint32_t ulLengthInBytes;                                                         
    uint32_t ulParameters;                                                            
} MemoryRegion_t;                                                                     

