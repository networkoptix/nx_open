#include "ret_chan_cmd.h"
#include "ret_chan_user.h"


pthread_mutex_t mutex;


void User_mutex_init()
{
#if UART_TEST
    pthread_mutex_init (&mutex,NULL);
#endif
}

void User_mutex_lock()
{
#if UART_TEST
    pthread_mutex_lock (&mutex);
#endif
}

void User_mutex_unlock()
{
#if UART_TEST
    pthread_mutex_unlock(&mutex);
#endif
}

void User_mutex_uninit()
{
#if UART_TEST
    pthread_mutex_destroy(&mutex);
#endif
}

void User_askUserSecurity(Security* security)
{
#if UART_TEST
	Byte userName[20] = "userName";
	Byte password[20] = "password";

	Cmd_StringClear(&security->userName);
	Cmd_StringClear(&security->password);

	Cmd_StringSet(userName, 20, &security->userName);
	Cmd_StringSet(password, 20, &security->password);
#endif
}

void * User_memory_allocate(size_t size)
{
#if UART_TEST
	void *tmp = NULL;
	(tmp) = malloc(size);
	return (tmp);
#endif
}
void User_memory_free(void* ptr)
{
#if UART_TEST
	if(ptr!=NULL)
	{
		free(ptr);
		ptr = NULL;
	}
#endif
}

void * User_memory_copy( void * destination, const void * source, size_t num )
{
#if UART_TEST
    return memcpy ( destination, source, num );
#endif
}

void * User_memory_set ( void * ptr, int value, size_t num )
{
#if UART_TEST
    return memset ( ptr, value, num );
#endif
}

Bool User_RCString_equal( const RCString * str1, const RCString * str2)
{
#if UART_TEST
	if(str1->stringLength > str2->stringLength)
		return False;
	if(str1->stringLength < str2->stringLength)
		return False;

	if( strcmp((char*)str1->stringData, (char*)str2->stringData) != 0)
		return False;

	return True;
#endif
}
