/**********************************************************
* Jun 14, 2016
* akolesnikov
***********************************************************/

#include "tunnel_connection_chosen_data.h"


namespace nx {
namespace hpm {
namespace api {

TunnelConnectionChosenRequest::TunnelConnectionChosenRequest()
:
    StunRequestData(kMethod)
{
}

void TunnelConnectionChosenRequest::serializeAttributes(nx::stun::Message* const /*message*/)
{
}

bool TunnelConnectionChosenRequest::parseAttributes(const nx::stun::Message& /*message*/)
{
    return true;
}


TunnelConnectionChosenResponse::TunnelConnectionChosenResponse()
:
    StunResponseData(kMethod)
{
}

void TunnelConnectionChosenResponse::serializeAttributes(nx::stun::Message* const /*message*/)
{
}

bool TunnelConnectionChosenResponse::parseAttributes(const nx::stun::Message& /*message*/)
{
    return true;
}

}   //namespace api
}   //namespace hpm
}   //namespace nx
