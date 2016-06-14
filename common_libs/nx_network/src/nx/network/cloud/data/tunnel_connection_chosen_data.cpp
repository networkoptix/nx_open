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
}

bool TunnelConnectionChosenRequest::parse(const nx::stun::Message& message)
{
    return true;
}

void TunnelConnectionChosenResponse::serialize(nx::stun::Message* const message)
{
}

bool TunnelConnectionChosenResponse::parse(const nx::stun::Message& message)
{
    return true;
}

}   //namespace api
}   //namespace hpm
}   //namespace nx
