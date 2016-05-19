/**********************************************************
* Jan 19, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <nx/network/stun/cc/custom_stun.h>
#include <nx/utils/log/log.h>
#include <nx/utils/move_only_func.h>

#include "data/result_code.h"


namespace nx {
namespace hpm {
namespace api {

/** Provides helper functions for easy adding requests to mediator */
template<class NetworkClientType>
class BaseMediatorClient
:
    public NetworkClientType
{
public:
    //TODO #ak #msvc2015 variadic template
    template<typename Arg1Type>
        BaseMediatorClient(Arg1Type arg1)
    :
        NetworkClientType(std::move(arg1))
    {
    }

protected:
    template<typename RequestData, typename CompletionHandlerType>
    void doRequest(
        nx::stun::cc::methods::Value method,
        RequestData requestData,
        CompletionHandlerType completionHandler)
    {
        nx::stun::Message request(
            nx::stun::Header(
                stun::MessageClass::request,
                method));
        requestData.serialize(&request);

        sendRequestAndReceiveResponse(
            std::move(request),
            std::move(completionHandler));
    }

    template<typename ResponseData>
    void sendRequestAndReceiveResponse(
        nx::stun::Message request,
        utils::MoveOnlyFunc<void(nx::hpm::api::ResultCode, ResponseData)> completionHandler)
    {
        using namespace nx::hpm::api;

        const nx::stun::cc::methods::Value method =
            static_cast<nx::stun::cc::methods::Value>(request.header.method);

        this->sendRequest(
            std::move(request),
            [this, method, completionHandler = std::move(completionHandler)](
                SystemError::ErrorCode code,
                stun::Message message)
        {
            if (code != SystemError::noError)
            {
                NX_LOGX(lm("Error performing %1 request to connection_mediator. %2").
                    arg(stun::cc::methods::toString(method)).
                    arg(SystemError::toString(code)),
                    cl_logDEBUG1);
                return completionHandler(
                    ResultCode::networkError,
                    ResponseData());
            }

            api::ResultCode resultCode = api::ResultCode::ok;
            const auto* resultCodeHeader = 
                message.getAttribute<nx::stun::cc::attrs::ResultCode>();
            if (resultCodeHeader)
                resultCode = resultCodeHeader->value();

            if (const auto error = message.hasError(code))
            {
                NX_LOGX(*error, cl_logDEBUG1);
                //TODO #ak get detailed error from response
                return completionHandler(
                    resultCodeHeader ? resultCode : api::ResultCode::otherLogicError,
                    ResponseData());
            }

            ResponseData responseData;
            if (!responseData.parse(message))
            {
                NX_LOGX(lm("Failed to parse %1 response: %2").
                    arg(nx::stun::cc::methods::toString(method)).
                    arg(responseData.errorText()), cl_logDEBUG1);
                return completionHandler(
                    ResultCode::responseParseError,
                    ResponseData());
            }

            completionHandler(
                resultCode,
                std::move(responseData));
        });
    }

    void sendRequestAndReceiveResponse(
        nx::stun::Message request,
        utils::MoveOnlyFunc<void(nx::hpm::api::ResultCode)> completionHandler)
    {
        using namespace nx::hpm::api;

        const nx::stun::cc::methods::Value method =
            static_cast<nx::stun::cc::methods::Value>(request.header.method);

        this->sendRequest(
            std::move(request),
            [this, method, completionHandler = std::move(completionHandler)](
                SystemError::ErrorCode code,
                nx::stun::Message message) mutable
        {
            if (code != SystemError::noError)
            {
                NX_LOGX(lm("Error performing %1 request to connection_mediator. %2").
                    arg(stun::cc::methods::toString(method)).
                    arg(SystemError::toString(code)), cl_logDEBUG1);
                return completionHandler(ResultCode::networkError);
            }

            api::ResultCode resultCode = api::ResultCode::ok;
            const auto* resultCodeHeader = 
                message.getAttribute<nx::stun::cc::attrs::ResultCode>();
            if (resultCodeHeader)
                resultCode = resultCodeHeader->value();

            if (const auto error = message.hasError(code))
            {
                NX_LOGX(*error, cl_logDEBUG1);
                //TODO #ak get detailed error from response
                return completionHandler(
                    resultCodeHeader
                    ? resultCode
                    : api::ResultCode::otherLogicError);
            }

            completionHandler(resultCode);
        });
    }
};

} // namespace api
} // namespace hpm
} // namespace nx
