// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/stun/extension/stun_extension_types.h>
#include <nx/utils/log/log.h>
#include <nx/utils/move_only_func.h>

#include "data/result_code.h"

namespace nx::hpm::api {

/**
 * Provides helper functions for easy adding requests to mediator.
 */
template<class NetworkClientType>
class BaseMediatorClient:
    public NetworkClientType
{
public:
    //TODO #akolesnikov #msvc2015 variadic template
    template<typename Arg1Type>
        BaseMediatorClient(Arg1Type arg1)
    :
        NetworkClientType(std::move(arg1))
    {
    }

protected:
    template<typename RequestData, typename CompletionHandlerType>
    void doRequest(
        RequestData requestData,
        CompletionHandlerType completionHandler)
    {
        network::stun::Message request(
            network::stun::Header(
                network::stun::MessageClass::request,
                RequestData::kMethod));
        requestData.serialize(&request);

        sendRequestAndReceiveResponse(
            std::move(request),
            std::move(completionHandler));
    }

    // TODO: #akolesnikov following two methods contain much code duplication.

    template<typename ResponseData>
    void sendRequestAndReceiveResponse(
        network::stun::Message request,
        utils::MoveOnlyFunc<void(network::stun::TransportHeader, nx::hpm::api::ResultCode, ResponseData)> completionHandler)
    {
        using namespace nx::hpm::api;
        const network::stun::extension::methods::Value method =
            static_cast<network::stun::extension::methods::Value>(request.header.method);
        NX_ASSERT(method == ResponseData::kMethod, "Request and response methods mismatch");

        this->sendRequest(
            std::move(request),
            [this, method, completionHandler = std::move(completionHandler)](
                SystemError::ErrorCode code,
                network::stun::Message message)
        {
            if (code != SystemError::noError)
            {
                NX_DEBUG(this, nx::format("Error performing %1 request to connection_mediator. %2").
                    arg(network::stun::extension::methods::toString(method)).
                    arg(SystemError::toString(code)));
                return completionHandler(
                    std::move(message.transportHeader),
                    ResultCode::networkError,
                    ResponseData());
            }

            api::ResultCode resultCode = api::ResultCode::ok;
            const auto* resultCodeHeader =
                message.getAttribute<network::stun::extension::attrs::ResultCode>();
            if (resultCodeHeader)
                resultCode = resultCodeHeader->value();

            if (const auto error = message.hasError(code))
            {
                NX_VERBOSE(this, *error);
                //TODO #akolesnikov get detailed error from response
                return completionHandler(
                    std::move(message.transportHeader),
                    resultCodeHeader ? resultCode : api::ResultCode::otherLogicError,
                    ResponseData());
            }

            ResponseData responseData;
            if (!responseData.parse(message))
            {
                NX_DEBUG(this, "Failed to parse %1 response: %2",
                    network::stun::extension::methods::toString(method),
                    responseData.errorText());
                return completionHandler(
                    std::move(message.transportHeader),
                    ResultCode::responseParseError,
                    ResponseData());
            }

            completionHandler(
                std::move(message.transportHeader),
                resultCode,
                std::move(responseData));
        });
    }

    template<typename ResponseData>
    void sendRequestAndReceiveResponse(
        network::stun::Message request,
        utils::MoveOnlyFunc<void(nx::hpm::api::ResultCode, ResponseData)> completionHandler)
    {
        sendRequestAndReceiveResponse<ResponseData>(
            std::move(request),
            nx::utils::MoveOnlyFunc<void(network::stun::TransportHeader, nx::hpm::api::ResultCode, ResponseData)>(
                [completionHandler = std::move(completionHandler)](
                    network::stun::TransportHeader /*transportHeader*/,
                    nx::hpm::api::ResultCode resultCode,
                    ResponseData responseData)
                {
                    completionHandler(resultCode, std::move(responseData));
                }));
    }

    void sendRequestAndReceiveResponse(
        network::stun::Message request,
        utils::MoveOnlyFunc<void(network::stun::TransportHeader, nx::hpm::api::ResultCode)> completionHandler)
    {
        using namespace nx::hpm::api;

        const network::stun::extension::methods::Value method =
            static_cast<network::stun::extension::methods::Value>(request.header.method);

        this->sendRequest(
            std::move(request),
            [this, method, completionHandler = std::move(completionHandler)](
                SystemError::ErrorCode code,
                network::stun::Message message) mutable
        {
            if (code != SystemError::noError)
            {
                NX_DEBUG(this, "Error performing %1 request to connection_mediator. %2",
                    network::stun::extension::methods::toString(method),
                    SystemError::toString(code));
                return completionHandler(std::move(message.transportHeader), ResultCode::networkError);
            }

            api::ResultCode resultCode = api::ResultCode::ok;
            const auto* resultCodeHeader =
                message.getAttribute<network::stun::extension::attrs::ResultCode>();
            if (resultCodeHeader)
                resultCode = resultCodeHeader->value();

            if (const auto error = message.hasError(code))
            {
                NX_VERBOSE(this, *error);
                //TODO #akolesnikov get detailed error from response
                return completionHandler(
                    std::move(message.transportHeader),
                    resultCodeHeader ? resultCode : api::ResultCode::otherLogicError);
            }

            completionHandler(std::move(message.transportHeader), resultCode);
        });
    }

    void sendRequestAndReceiveResponse(
        network::stun::Message request,
        utils::MoveOnlyFunc<void(nx::hpm::api::ResultCode)> completionHandler)
    {
        sendRequestAndReceiveResponse(
            std::move(request),
            nx::utils::MoveOnlyFunc<void(network::stun::TransportHeader, nx::hpm::api::ResultCode)>(
                [completionHandler = std::move(completionHandler)](
                    network::stun::TransportHeader /*transportHeader*/,
                    nx::hpm::api::ResultCode resultCode)
                {
                    completionHandler(resultCode);
                }));
    }
};

} // namespace nx::hpm::api
