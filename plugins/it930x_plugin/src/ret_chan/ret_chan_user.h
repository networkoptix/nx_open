#ifndef RETURN_CHANNEL_USER_H
#define RETURN_CHANNEL_USER_H

#include "ret_chan_cmd.h"

unsigned User_returnChannelBusTx(IN unsigned bufferLength, IN const Byte * buffer, OUT int * txLen);
unsigned User_returnChannelBusRx(IN unsigned bufferLength, OUT Byte * buffer, OUT int * rxLen);

void User_askUserSecurity(IN Security * security);
void * User_memory_allocate(IN size_t size);
void User_memory_free(IN void * ptr);
void *User_memory_copy(OUT void * destination, IN const void * source, IN size_t num);
void * User_memory_set(OUT void * ptr, IN int value, IN size_t num);

Bool User_RCString_equal(IN const RCString * str1, IN const RCString * str2);

void User_mutex_init();
void User_mutex_lock();
void User_mutex_unlock();
void User_mutex_uninit();

#endif
