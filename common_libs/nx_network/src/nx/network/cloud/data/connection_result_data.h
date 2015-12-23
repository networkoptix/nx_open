/**********************************************************
* Dec 23, 2015
* akolesnikov
***********************************************************/

#ifndef NX_MEDIATOR_API_CONNECTION_RESULT_DATA_H
#define NX_MEDIATOR_API_CONNECTION_RESULT_DATA_H

#include "stun_message_data.h"


namespace nx {
namespace hpm {
namespace api {

class NX_NETWORK_API ConnectionResultRequest
:
    public StunMessageData
{
public:
    //TODO #ak add some statistics useful for later analysis
    bool connectionSucceeded;

    ConnectionResultRequest(bool _connectionSucceeded);

    void serialize(nx::stun::Message* const message);
    bool parse(const nx::stun::Message& message);
};

}   //api
}   //hpm
}   //nx

#endif  //NX_MEDIATOR_API_CONNECTION_RESULT_DATA_H
