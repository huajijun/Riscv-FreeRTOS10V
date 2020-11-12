#include <common.h>
BaseType_t xStartContext[31] = {0};
void memcpy(char * des , char * src, int number)
{
	char *tmp = des;
	char* tmp2 = src;
	for(int i = 0; i< number; i++)
	{
		*tmp++ = *tmp2++;
	}
}
