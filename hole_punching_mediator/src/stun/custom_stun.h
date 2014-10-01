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
    namespace StunParameters 
    {
        enum Value 
        {
            systemName = static_cast<int>(nx_stun::attr::AttributeType::userDefine),
            authorization,
            serverId
        };
        // I just assume that we will use UnknownAttribute as our way to extend the 
        // parser/serializer class pair. Maybe other "factory" based method works ,
        // but this may be simpler for us.
        struct SystemName  {
            bool parse( const nx_stun::attr::UnknownAttribute& ) { return false; }
            bool serialize( nx_stun::attr::UnknownAttribute* );
            std::string system_name;
        };
    }
}

#endif  //NX_CUSTOM_STUN_H
