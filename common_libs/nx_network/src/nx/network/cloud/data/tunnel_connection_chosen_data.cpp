/**********************************************************
* Jun 14, 2016
* akolesnikov
***********************************************************/

#include "tunnel_connection_chosen_data.h"


namespace nx {
namespace hpm {
namespace api {

void TunnelConnectionChosenRequest::serialize(nx::stun::Message* const message)
{
    message->header = nx::stun::Header(
        nx::stun::MessageClass::request,
        stun::cc::methods::tunnelConnectionChosen);
}

bool TunnelConnectionChosenRequest::parse(const nx::stun::Message& /*message*/)
{
    return true;
}

void TunnelConnectionChosenResponse::serialize(nx::stun::Message* const message)
{
    message->header = nx::stun::Header(
        nx::stun::MessageClass::successResponse,
        stun::cc::methods::tunnelConnectionChosen);
}

bool TunnelConnectionChosenResponse::parse(const nx::stun::Message& /*message*/)
{
    return true;
}

}   //namespace api
}   //namespace hpm
}   //namespace nx
