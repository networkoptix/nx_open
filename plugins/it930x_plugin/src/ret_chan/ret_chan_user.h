#ifndef RETURN_CHANNEL_USER_H
#define RETURN_CHANNEL_USER_H

#include "ret_chan_cmd.h"

void User_askUserSecurity(IN Security * security);

void * User_memory_allocate(IN size_t size);
void User_memory_free(IN void * ptr);
void *User_memory_copy(OUT void * destination, IN const void * source, IN size_t num);
void * User_memory_set(OUT void * ptr, IN int value, IN size_t num);

#endif
