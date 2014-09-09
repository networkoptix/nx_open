/**********************************************************
* 9 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_CUSTOM_STUN_H
#define NX_CUSTOM_STUN_H

#include <utils/network/stun/stun_message.h>


//!hpm stands for "Hole Punching Mediator". This namespace is to contain all mediator types (maybe)
namespace nx_hpm
{
    namespace StunMethods
    {
        enum Value
        {
            bind = nx_stun::MethodType::user_method,
            connect
        };
    };

    //TODO custom stun requests parameters
}

#endif  //NX_CUSTOM_STUN_H
