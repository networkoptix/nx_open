/**********************************************************
* Dec 23, 2015
* akolesnikov
***********************************************************/

#include "connection_result_data.h"


namespace nx {
namespace hpm {
namespace api {

ConnectionResultRequest::ConnectionResultRequest()
:
    connectionSucceeded(false)
{
}

ConnectionResultRequest::ConnectionResultRequest(bool _connectionSucceeded)
:
    connectionSucceeded(_connectionSucceeded)
{
}

void ConnectionResultRequest::serialize(nx::stun::Message* const message)
{
    message->newAttribute<stun::cc::attrs::ConnectionSucceeded>(
        connectionSucceeded ? "true" : "false");
}

bool ConnectionResultRequest::parse(const nx::stun::Message& message)
{
    nx::String connectionSucceededStr;
    if (!readStringAttributeValue<stun::cc::attrs::ConnectionSucceeded>(
            message, &connectionSucceededStr))
    {
        return false;
    }
    connectionSucceeded = connectionSucceededStr == "true";

    return true;
}

}   //api
}   //hpm
}   //nx
