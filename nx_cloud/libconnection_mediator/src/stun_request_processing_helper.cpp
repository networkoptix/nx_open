/**********************************************************
* Dec 23, 2015
* akolesnikov
***********************************************************/

#include "stun_request_processing_helper.h"


namespace nx {
namespace hpm {

void sendSuccessResponse(
    const ConnectionSharedPtr& connection,
    stun::Header& request)
{
    stun::Message response(
        stun::Header(
            stun::MessageClass::successResponse,
            request.method,
            std::move(request.transactionId)));

    connection->sendMessage(std::move(response));
}

void sendErrorResponse(
    const ConnectionSharedPtr& connection,
    stun::Header& request,
    int code,
    String reason)
{
    stun::Message response(
        stun::Header(
            stun::MessageClass::errorResponse,
            request.method,
            std::move(request.transactionId)));

    response.newAttribute< stun::attrs::ErrorDescription >(
        code,
        std::move(reason));
    connection->sendMessage(std::move(response));
}

}   //hpm
}   //nx
