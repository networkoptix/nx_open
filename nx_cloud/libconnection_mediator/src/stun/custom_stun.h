/**********************************************************
* 9 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_CUSTOM_STUN_H
#define NX_CUSTOM_STUN_H

#include <utils/network/stun/stun_message.h>
#include <utils/common/uuid.h>

namespace nx {
namespace hpm { // hpm stands for "Hole Punching Mediator"

namespace Methods
{
    enum Value
    {
        listen = stun::MethodType::userMethod,
        connect,
        connectionRequested
    };
};

//TODO custom stun requests parameters
namespace Attributes
{
    enum Value
    {
        systemName = stun::attr::AttributeType::userDefine,
        authorization,
        serverId
    };
}

namespace attr
{
    struct SystemName : stun::attr::UnknownAttribute
    {
        SystemName( nx::String value )
            : stun::attr::UnknownAttribute( s_type, value ) {}

        static const int s_type = Attributes::systemName;
    };

    struct Authorization : stun::attr::UnknownAttribute
    {
        Authorization( nx::String value )
            : stun::attr::UnknownAttribute( s_type, value ) {}

        // TODO: introduce some way of parsing subattributes
        static const int s_type = Attributes::authorization;
    };

    struct ServerId : stun::attr::UnknownAttribute
    {
        ServerId( QnUuid value )
            : stun::attr::UnknownAttribute( s_type, value.toRfc4122() ) {}

        QnUuid uuid() const { return QnUuid::fromRfc4122( value ); }

        static const int s_type = Attributes::serverId;
    };
}

} // namespace hpm
} // namespace nx

#endif  //NX_CUSTOM_STUN_H
