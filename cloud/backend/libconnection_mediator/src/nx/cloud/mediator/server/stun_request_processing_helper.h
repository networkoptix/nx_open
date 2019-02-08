#pragma once

#include <type_traits>

#include <nx/network/cloud/data/result_code.h>
#include <nx/network/stun/server_connection.h>
#include <nx/fusion/serialization/lexical.h>
#include <nx/utils/type_utils.h>

namespace nx {
namespace hpm {

using ConnectionPtr = network::stun::AbstractServerConnection*;
using ConnectionStrongRef = std::shared_ptr<network::stun::AbstractServerConnection>;
using ConnectionWeakRef = std::weak_ptr<network::stun::AbstractServerConnection>;

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

struct RequestSourceDescriptor
{
    network::TransportProtocol transportProtocol;
    nx::network::SocketAddress sourceAddress;

    RequestSourceDescriptor() = delete;
};

namespace detail {

template<
    typename ConnectionStrongRef,
    typename... OutputData>
void fillAndSendResponse(
    network::stun::Header requestHeader,
    const ConnectionStrongRef& connection,
    api::ResultCode resultCode,
    OutputData*... outputData)
{
    if (resultCode != api::ResultCode::ok)
    {
        return sendErrorResponse(
            connection,
            std::move(requestHeader),
            resultCode,
            api::resultCodeToStunErrorCode(resultCode),
            QnLexical::serialized(resultCode).toLatin1());
    }

    network::stun::Message response(
        network::stun::Header(
            network::stun::MessageClass::successResponse,
            requestHeader.method,
            std::move(requestHeader.transactionId)));

    if constexpr (sizeof...(OutputData) > 0)
    {
        serialize(outputData..., &response);
    }

    response.newAttribute<network::stun::extension::attrs::ResultCode>(resultCode);

    connection->sendMessage(std::move(response));
}

/**
 * - Reads input data out of STUN request.
 * - Passes it to the member function processingFunc of processor.
 * - On processing done sends response.
 */
template<typename InputData, typename... OutputData>
void processRequest(
    typename nx::utils::identity<std::function<void(
        const ConnectionStrongRef&,
        network::stun::Message,
        InputData,
        std::function<void(api::ResultCode, OutputData...)>)>>::type processorFunc,
    const ConnectionStrongRef& connection,
    network::stun::Message request)
{
    InputData input;
    if (!input.parse(request))
    {
        return sendErrorResponse(
            connection,
            std::move(request.header),
            api::ResultCode::badRequest,
            network::stun::error::badRequest,
            input.errorText());
    }

    auto requestHeader = request.header;
    if (connection->transportProtocol() == nx::network::TransportProtocol::udp)
    {
        processorFunc(
            connection,
            request,
            std::move(input),
            [requestHeader = std::move(requestHeader), connection](
                api::ResultCode resultCode,
                OutputData... outputData) mutable
            {
                fillAndSendResponse(
                    requestHeader,
                    connection,
                    resultCode,
                    &outputData...);
            });
    }
    else
    {
        ConnectionWeakRef weakConnectionRef = connection;
        processorFunc(
            connection,
            request,
            std::move(input),
            [requestHeader = std::move(requestHeader), weakConnectionRef](
                api::ResultCode resultCode,
                OutputData... outputData) mutable
            {
                // Connection can be removed at any moment.
                auto connectionStrongRef = weakConnectionRef.lock();
                if (!connectionStrongRef)
                    return;
                fillAndSendResponse(
                    requestHeader,
                    connectionStrongRef,
                    resultCode,
                    &outputData...);
            });
    }
}

template<typename Processor, typename InputData, typename... OutputData>
auto createRequestProcessor(
    void (Processor::*func)(
        const ConnectionStrongRef&,
        network::stun::Message,
        InputData,
        std::function<void(api::ResultCode, OutputData...)>),
    Processor* processor)
{
    return
        [processor, func](
            const ConnectionStrongRef& connection,
            network::stun::Message message)
        {
            processRequest<InputData, OutputData...>(
                [processor, func](auto... args)
                {
                    (processor->*func)(std::move(args)...);
                },
                connection,
                std::move(message));
        };
}

template<typename Processor, typename InputData, typename... OutputData>
auto createRequestProcessor(
    void (Processor::*func)(
        const ConnectionStrongRef&,
        InputData,
        std::function<void(api::ResultCode, OutputData...)>),
    Processor* processor)
{
    return
        [processor, func](
            const ConnectionStrongRef& connection,
            network::stun::Message message)
        {
            processRequest<InputData, OutputData...>(
                [processor, func](
                    const ConnectionStrongRef& connection,
                    network::stun::Message,
                    auto... args)
                {
                    (processor->*func)(connection, std::move(args)...);
                },
                connection,
                std::move(message));
        };
}


template<typename Processor, typename InputData, typename... OutputData>
auto createRequestProcessor(
    void (Processor::*func)(
        const RequestSourceDescriptor&,
        InputData,
        std::function<void(api::ResultCode, OutputData...)>),
    Processor* processor)
{
    return
        [processor, func](
            const ConnectionStrongRef& connection,
            network::stun::Message message)
        {
            processRequest<InputData, OutputData...>(
                [processor, func](
                    const ConnectionStrongRef& connection,
                    network::stun::Message,
                    auto... args)
                {
                    (processor->*func)(
                        RequestSourceDescriptor{
                            connection->transportProtocol(),
                            connection->getSourceAddress()},
                        std::move(args)...);
                },
                connection,
                std::move(message));
        };
}

} // namespace detail

} // namespace hpm
} // namespace nx
