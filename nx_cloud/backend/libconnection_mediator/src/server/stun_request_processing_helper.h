#pragma once

#include <type_traits>

#include <nx/network/cloud/data/result_code.h>
#include <nx/network/stun/server_connection.h>
#include <nx/fusion/serialization/lexical.h>

namespace nx {
namespace hpm {

typedef std::shared_ptr< network::stun::AbstractServerConnection > ConnectionStrongRef;
typedef std::weak_ptr< network::stun::AbstractServerConnection > ConnectionWeakRef;

/** Send success responce without attributes. */
template<typename ConnectionPtr>
void sendSuccessResponse(
    const ConnectionPtr& connection,
    network::stun::Header requestHeader)
{
    network::stun::Message response(
        network::stun::Header(
            network::stun::MessageClass::successResponse,
            requestHeader.method,
            std::move(requestHeader.transactionId)));
    response.newAttribute<network::stun::extension::attrs::ResultCode>(api::ResultCode::ok);

    connection->sendMessage(std::move(response), nullptr);
}

/** Send error responce with error code and description as attribute. */
template<typename ConnectionPtr>
void sendErrorResponse(
    const ConnectionPtr& connection,
    network::stun::Header requestHeader,
    api::ResultCode resultCode,
    int stunErrorCode,
    String reason)
{
    network::stun::Message response(
        network::stun::Header(
            network::stun::MessageClass::errorResponse,
            requestHeader.method,
            std::move(requestHeader.transactionId)));

    response.newAttribute<network::stun::extension::attrs::ResultCode>(resultCode);
    response.newAttribute< network::stun::attrs::ErrorCode >(
        stunErrorCode,
        std::move(reason));
    connection->sendMessage(std::move(response), nullptr);
}

template<typename OutputData>
void serialize(
    OutputData* outputData,
    network::stun::Message* const response,
    typename std::enable_if<!std::is_void<OutputData>::value>::type* = nullptr)
{
    outputData->serialize(response);
}

template<typename OutputData>
void serialize(
    OutputData* /*outputData*/,
    network::stun::Message* const /*response*/,
    typename std::enable_if<std::is_void<OutputData>::value>::type* = nullptr)
{
}

template<
    typename ConnectionStrongRef,
    typename OutputData>
void fillAndSendResponse(
    network::stun::Header requestHeader,
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

    network::stun::Message response(
        network::stun::Header(
            network::stun::MessageClass::successResponse,
            requestHeader.method,
            std::move(requestHeader.transactionId)));
    serialize(outputData, &response);
    response.newAttribute<network::stun::extension::attrs::ResultCode>(resultCode);

    connection->sendMessage(std::move(response));
}

/**
 * - Reads input data out of STUN request.
 * - Passes it to the member function processingFunc of processor.
 * - On processing done sends response.
 * TODO: #ak Come up with a single implementation when variadic templates are available.
 */
template<
    typename ProcessorType,
    typename InputData>
void processRequestWithNoOutput(
    void (ProcessorType::*processingFunc)(
        const ConnectionStrongRef&,
        InputData,
        network::stun::Message,
        std::function<void(api::ResultCode)>),
    ProcessorType* processor,
    const ConnectionStrongRef& connection,
    network::stun::Message request)
{
    InputData input;
    if (!input.parse(request))
        return sendErrorResponse(
            connection,
            std::move(request.header),
            api::ResultCode::badRequest,
            network::stun::error::badRequest,
            input.errorText());

    auto requestHeader = request.header;
    if (connection->transportProtocol() == nx::network::TransportProtocol::udp)
    {
        // Holding ownership of connection until request processing completion.
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
        // Holding ownership of connection until request processing completion.
        (processor->*processingFunc)(
            connection,
            std::move(input),
            request,
            [/*std::move*/ requestHeader, weakConnectionRef](api::ResultCode resultCode) mutable
            {
                // Connection can be removed at any moment.
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
        network::stun::Message,
        std::function<void(api::ResultCode, OutputData)>),
    ProcessorType* processor,
    const ConnectionStrongRef& connection,
    network::stun::Message request)
{
    InputData input;
    if (!input.parse(request))
        return sendErrorResponse(
            connection,
            std::move(request.header),
            api::ResultCode::badRequest,
            network::stun::error::badRequest,
            input.errorText());

    auto requestHeader = request.header;
    if (connection->transportProtocol() == nx::network::TransportProtocol::udp)
    {
        // Holding ownership of connection until request processing completion.
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
        // Holding ownership of connection until request processing completion.
        (processor->*processingFunc)(
            connection,
            std::move(input),
            request,
            [/*std::move*/ requestHeader, weakConnectionRef](
                api::ResultCode resultCode,
                OutputData outputData) mutable
            {
                // Connection can be removed at any moment.
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

} // namespace hpm
} // namespace nx
