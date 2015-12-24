/**********************************************************
* Dec 23, 2015
* akolesnikov
***********************************************************/

#ifndef NX_MEDIATOR_STUN_REQUEST_PROCESSING_HELPER_H
#define NX_MEDIATOR_STUN_REQUEST_PROCESSING_HELPER_H

#include <nx/network/cloud/data/result_code.h>
#include <nx/network/stun/server_connection.h>
#include <utils/serialization/lexical.h>


namespace nx {
namespace hpm {

typedef std::shared_ptr< stun::ServerConnection > ConnectionSharedPtr;
typedef std::weak_ptr< stun::ServerConnection > ConnectionWeakPtr;


/** Send success responce without attributes */
void sendSuccessResponse(
    const ConnectionSharedPtr& connection,
    stun::Header requestHeader);

/** Send error responce with error code and description as attribute */
void sendErrorResponse(
    const ConnectionSharedPtr& connection,
    stun::Header requestHeader,
    int code,
    String reason);


//TODO #ak come up with a single implementation when variadic templates are available

template<typename ProcessorType, typename InputData>
void processRequestWithNoOutput(
    void (ProcessorType::*processingFunc)(
        ConnectionSharedPtr,
        InputData,
        std::function<void(api::ResultCode)>),
    ProcessorType* processor,
    ConnectionSharedPtr connection,
    stun::Message request)
{
    InputData input;
    if (!input.parse(request))
        return sendErrorResponse(
            connection,
            std::move(request.header),
            nx::stun::error::badRequest,
            input.errorText());

    ConnectionWeakPtr connectionWeak = connection;

    auto requestHeader = std::move(request.header);
    (processor->*processingFunc)(
        connection,
        std::move(input),
        [/*std::move*/ requestHeader, connectionWeak](api::ResultCode resultCode) mutable
    {
        auto connection = connectionWeak.lock();
        if (!connection)
            return;

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

template<typename ProcessorType, typename InputData, typename OutputData>
void processRequestWithOutput(
    void (ProcessorType::*processingFunc)(
        ConnectionSharedPtr,
        InputData,
        std::function<void(api::ResultCode, OutputData)>),
    ProcessorType* processor,
    ConnectionSharedPtr connection,
    stun::Message request)
{
    InputData input;
    if (!input.parse(request))
        return sendErrorResponse(
            connection,
            std::move(request.header),
            nx::stun::error::badRequest,
            input.errorText());

    ConnectionWeakPtr connectionWeak = connection;

    auto requestHeader = std::move(request.header);
    (processor->*processingFunc)(
        connection,
        std::move(input),
        [/*std::move*/ requestHeader, connectionWeak](
            api::ResultCode resultCode,
            OutputData outputData) mutable
    {
        auto connection = connectionWeak.lock();
        if (!connection)
            return;

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
