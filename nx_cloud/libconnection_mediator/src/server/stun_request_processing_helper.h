/**********************************************************
* Dec 23, 2015
* akolesnikov
***********************************************************/

#ifndef NX_MEDIATOR_STUN_REQUEST_PROCESSING_HELPER_H
#define NX_MEDIATOR_STUN_REQUEST_PROCESSING_HELPER_H

#include <type_traits>

#include <nx/network/cloud/data/result_code.h>
#include <nx/network/stun/server_connection.h>
#include <nx/fusion/serialization/lexical.h>


namespace nx {
namespace hpm {

typedef std::shared_ptr< nx::stun::AbstractServerConnection > ConnectionStrongRef;
typedef std::weak_ptr< nx::stun::AbstractServerConnection > ConnectionWeakRef;

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
    response.newAttribute<stun::extension::attrs::ResultCode>(api::ResultCode::ok);

    connection->sendMessage(std::move(response), nullptr);
}

/** Send error responce with error code and description as attribute */
template<typename ConnectionPtr>
void sendErrorResponse(
    const ConnectionPtr& connection,
    stun::Header requestHeader,
    api::ResultCode resultCode,
    int stunErrorCode,
    String reason)
{
    stun::Message response(
        stun::Header(
            stun::MessageClass::errorResponse,
            requestHeader.method,
            std::move(requestHeader.transactionId)));

    response.newAttribute<stun::extension::attrs::ResultCode>(resultCode);
    response.newAttribute< stun::attrs::ErrorCode >(
        stunErrorCode,
        std::move(reason));
    connection->sendMessage(std::move(response), nullptr);
}

template<typename OutputData>
void serialize(
    OutputData* outputData,
    stun::Message* const response,
    typename std::enable_if<!std::is_void<OutputData>::value>::type* = nullptr)
{
    outputData->serialize(response);
}

template<typename OutputData>
void serialize(
    OutputData* /*outputData*/,
    stun::Message* const /*response*/,
    typename std::enable_if<std::is_void<OutputData>::value>::type* = nullptr)
{
}

template<
    typename ConnectionStrongRef,
    typename OutputData>
void fillAndSendResponse(
    nx::stun::Header requestHeader,
    const ConnectionStrongRef& connection,
    api::ResultCode resultCode,
    OutputData* outputData = nullptr)
{
    if (resultCode != api::ResultCode::ok)
        return sendErrorResponse(
            connection,
            std::move(requestHeader),
            resultCode,
            api::resultCodeToStunErrorCode(resultCode),
            QnLexical::serialized(resultCode).toLatin1());

    stun::Message response(
        stun::Header(
            stun::MessageClass::successResponse,
            requestHeader.method,
            std::move(requestHeader.transactionId)));
    serialize(outputData, &response);
    response.newAttribute<stun::extension::attrs::ResultCode>(resultCode);

    connection->sendMessage(std::move(response));
}


/**
    Does following:\n
    - reads input data out of STUN \a request
    - passes it to the member function \a processingFunc of \a processor
    - on processin done sends response
    //TODO #ak come up with a single implementation when variadic templates are available
*/
template<
    typename ProcessorType,
    typename InputData>
void processRequestWithNoOutput(
    void (ProcessorType::*processingFunc)(
        const ConnectionStrongRef&,
        InputData,
        nx::stun::Message,
        std::function<void(api::ResultCode)>),
    ProcessorType* processor,
    const ConnectionStrongRef& connection,
    stun::Message request)
{
    InputData input;
    if (!input.parse(request))
        return sendErrorResponse(
            connection,
            std::move(request.header),
            api::ResultCode::badRequest,
            nx::stun::error::badRequest,
            input.errorText());

    auto requestHeader = request.header;
    if (connection->transportProtocol() == nx::network::TransportProtocol::udp)
    {
        //holding ownership of connection until request processing completion
        (processor->*processingFunc)(
            connection,
            std::move(input),
            request,
            [/*std::move*/ requestHeader, connection](api::ResultCode resultCode) mutable
            {
                fillAndSendResponse<ConnectionStrongRef, void>(
                    requestHeader,
                    connection,
                    resultCode);
            });
    }
    else
    {
        ConnectionWeakRef weakConnectionRef = connection;
        //holding ownership of connection until request processing completion
        (processor->*processingFunc)(
            connection,
            std::move(input),
            request,
            [/*std::move*/ requestHeader, weakConnectionRef](api::ResultCode resultCode) mutable
            {
                //connection can be removed at any moment
                auto connectionStrongRef = weakConnectionRef.lock();
                if (!connectionStrongRef)
                    return;
                fillAndSendResponse<ConnectionStrongRef, void>(
                    requestHeader,
                    connectionStrongRef,
                    resultCode);
            });
    }
}

template<
    typename ProcessorType,
    typename InputData,
    typename OutputData>
void processRequestWithOutput(
    void (ProcessorType::*processingFunc)(
        const ConnectionStrongRef&,
        InputData,
        nx::stun::Message,
        std::function<void(api::ResultCode, OutputData)>),
    ProcessorType* processor,
    const ConnectionStrongRef& connection,
    stun::Message request)
{
    InputData input;
    if (!input.parse(request))
        return sendErrorResponse(
            connection,
            std::move(request.header),
            api::ResultCode::badRequest,
            nx::stun::error::badRequest,
            input.errorText());

    auto requestHeader = request.header;
    if (connection->transportProtocol() == nx::network::TransportProtocol::udp)
    {
        //holding ownership of connection until request processing completion
        (processor->*processingFunc)(
            connection,
            std::move(input),
            request,
            [/*std::move*/ requestHeader, connection](
                api::ResultCode resultCode,
                OutputData outputData) mutable
            {
                fillAndSendResponse<ConnectionStrongRef, OutputData>(
                    requestHeader,
                    connection,
                    resultCode,
                    &outputData);
            });
    }
    else
    {
        ConnectionWeakRef weakConnectionRef = connection;
        //holding ownership of connection until request processing completion
        (processor->*processingFunc)(
            connection,
            std::move(input),
            request,
            [/*std::move*/ requestHeader, weakConnectionRef](
                api::ResultCode resultCode,
                OutputData outputData) mutable
            {
                //connection can be removed at any moment
                auto connectionStrongRef = weakConnectionRef.lock();
                if (!connectionStrongRef)
                    return;
                fillAndSendResponse<ConnectionStrongRef, OutputData>(
                    requestHeader,
                    connectionStrongRef,
                    resultCode,
                    &outputData);
            });
    }
}

}   //hpm
}   //nx

#endif  //NX_MEDIATOR_STUN_REQUEST_PROCESSING_HELPER_H
