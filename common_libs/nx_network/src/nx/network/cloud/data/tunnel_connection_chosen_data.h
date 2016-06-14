/**********************************************************
* Jun 14, 2016
* akolesnikov
***********************************************************/

#pragma once

#include "stun_message_data.h"


namespace nx {
namespace hpm {
namespace api {

class NX_NETWORK_API TunnelConnectionChosenRequest
:
    public StunMessageData
{
public:
    void serialize(nx::stun::Message* const message);
    bool parse(const nx::stun::Message& message);
};

class NX_NETWORK_API TunnelConnectionChosenResponse
:
    public StunMessageData
{
public:
    void serialize(nx::stun::Message* const message);
    bool parse(const nx::stun::Message& message);
};

}   //namespace api
}   //namespace hpm
}   //namespace nx
