/**********************************************************
* Dec 23, 2015
* akolesnikov
***********************************************************/

#include "stun_request_processing_helper.h"


namespace nx {
namespace hpm {

void sendSuccessResponse(
    const ConnectionSharedPtr& connection,
    stun::Header requestHeader)
{
    stun::Message response(
        stun::Header(
            stun::MessageClass::successResponse,
            requestHeader.method,
            std::move(requestHeader.transactionId)));

    connection->sendMessage(std::move(response));
}

void sendErrorResponse(
    const ConnectionSharedPtr& connection,
    stun::Header requestHeader,
    int code,
    String reason)
{
    stun::Message response(
        stun::Header(
            stun::MessageClass::errorResponse,
            requestHeader.method,
            std::move(requestHeader.transactionId)));

    response.newAttribute< stun::attrs::ErrorDescription >(
        code,
        std::move(reason));
    connection->sendMessage(std::move(response));
}

}   //hpm
}   //nx
