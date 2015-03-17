#include "ret_chan_cmd.h"
#include "ret_chan_user.h"


void User_askUserSecurity(Security * security)
{
	Byte userName[20] = "userName";
	Byte password[20] = "password";

	Cmd_StringClear(&security->userName);
	Cmd_StringClear(&security->password);

	Cmd_StringSet(userName, 20, &security->userName);
	Cmd_StringSet(password, 20, &security->password);
}

//

void * User_memory_allocate(size_t size)
{
    return malloc(size);
}

void User_memory_free(void * ptr)
{
    if (ptr != NULL)
	{
		free(ptr);
		ptr = NULL;
	}
}

void * User_memory_copy(void * destination, const void * source, size_t num)
{
    return memcpy(destination, source, num);
}

void * User_memory_set(void * ptr, int value, size_t num)
{
    return memset(ptr, value, num);
}
