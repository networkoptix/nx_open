/**********************************************************
* Dec 23, 2015
* akolesnikov
***********************************************************/

#ifndef NX_MEDIATOR_STUN_REQUEST_PROCESSING_HELPER_H
#define NX_MEDIATOR_STUN_REQUEST_PROCESSING_HELPER_H

#include <nx/network/cloud/data/result_code.h>
#include <nx/network/stun/server_connection.h>
#include <utils/serialization/lexical.h>

#include "connection.h"


namespace nx {
namespace hpm {

typedef std::shared_ptr< AbstractServerConnection > ConnectionStrongRef;
typedef std::weak_ptr< AbstractServerConnection > ConnectionWeakRef;


/** Send success responce without attributes */
template<typename ConnectionPtr>
void sendSuccessResponse(
    const ConnectionPtr& connection,
    stun::Header requestHeader)
{
    stun::Message response(
        stun::Header(
            stun::MessageClass::successResponse,
            requestHeader.method,
            std::move(requestHeader.transactionId)));

    connection->sendMessage(std::move(response));
}

/** Send error responce with error code and description as attribute */
template<typename ConnectionPtr>
void sendErrorResponse(
    const ConnectionPtr& connection,
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


//TODO #ak come up with a single implementation when variadic templates are available

template<
    typename ProcessorType,
    typename ConnectionStrongRef,
    typename InputData>
void processRequestWithNoOutput(
    void (ProcessorType::*processingFunc)(
        ConnectionStrongRef,
        InputData,
        std::function<void(api::ResultCode)>),
    ProcessorType* processor,
    ConnectionStrongRef connection,
    stun::Message request)
{
    InputData input;
    if (!input.parse(request))
        return sendErrorResponse(
            connection,
            std::move(request.header),
            nx::stun::error::badRequest,
            input.errorText());

    auto requestHeader = std::move(request.header);
    (processor->*processingFunc)(
        connection,
        std::move(input),
        [/*std::move*/ requestHeader, connection](api::ResultCode resultCode) mutable
    {
        if (resultCode != api::ResultCode::ok)
            return sendErrorResponse(
                connection,
                std::move(requestHeader),
                api::resultCodeToStunErrorCode(resultCode),
                QnLexical::serialized(resultCode).toLatin1());

        stun::Message response(
            stun::Header(
                stun::MessageClass::successResponse,
                requestHeader.method,
                std::move(requestHeader.transactionId)));

        connection->sendMessage(std::move(response));
    });
}

template<
    typename ProcessorType,
    typename ConnectionStrongRef,
    typename InputData,
    typename OutputData>
void processRequestWithOutput(
    void (ProcessorType::*processingFunc)(
        ConnectionStrongRef,
        InputData,
        std::function<void(api::ResultCode, OutputData)>),
    ProcessorType* processor,
    ConnectionStrongRef connection,
    stun::Message request)
{
    InputData input;
    if (!input.parse(request))
        return sendErrorResponse(
            connection,
            std::move(request.header),
            nx::stun::error::badRequest,
            input.errorText());

    auto requestHeader = std::move(request.header);
    (processor->*processingFunc)(
        connection,
        std::move(input),
        [/*std::move*/ requestHeader, connection](
            api::ResultCode resultCode,
            OutputData outputData) mutable
    {
        if (resultCode != api::ResultCode::ok)
            return sendErrorResponse(
                connection,
                std::move(requestHeader),
                api::resultCodeToStunErrorCode(resultCode),
                QnLexical::serialized(resultCode).toLatin1());

        stun::Message response(
            stun::Header(
                stun::MessageClass::successResponse,
                requestHeader.method,
                std::move(requestHeader.transactionId)));
        outputData.serialize(&response);

        connection->sendMessage(std::move(response));
    });
}

}   //hpm
}   //nx

#endif  //NX_MEDIATOR_STUN_REQUEST_PROCESSING_HELPER_H
