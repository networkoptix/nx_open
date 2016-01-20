/**********************************************************
* Jan 19, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <nx/network/stun/cc/custom_stun.h>
#include <nx/utils/log/log.h>

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
        std::function<void(nx::hpm::api::ResultCode, ResponseData)> completionHandler)
    {
        using namespace nx::hpm::api;

        const nx::stun::cc::methods::Value method =
            static_cast<nx::stun::cc::methods::Value>(request.header.method);

        sendRequest(
            std::move(request),
            [this, method, /*std::move*/ completionHandler](    //TODO #ak #msvc2015 move to lambda
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

            if (const auto error = nx::stun::AsyncClient::hasError(code, message))
            {
                ResultCode resultCode = 
                    ResultCode::otherLogicError;
                if (const auto err = message.getAttribute<nx::stun::attrs::ErrorDescription>())
                    resultCode = fromStunErrorToResultCode(*err);

                NX_LOGX(*error, cl_logDEBUG1);
                //TODO #ak get detailed error from response
                return completionHandler(resultCode, ResponseData());
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
                ResultCode::ok,
                std::move(responseData));
        });
    }

    void sendRequestAndReceiveResponse(
        nx::stun::Message request,
        std::function<void(nx::hpm::api::ResultCode)> completionHandler)
    {
        using namespace nx::hpm::api;

        const nx::stun::cc::methods::Value method =
            static_cast<nx::stun::cc::methods::Value>(request.header.method);

        sendRequest(
            std::move(request),
            [this, method, /*std::move*/ completionHandler](
                SystemError::ErrorCode code,
                nx::stun::Message message)
        {
            if (code != SystemError::noError)
            {
                NX_LOGX(lm("Error performing %1 request to connection_mediator. %2").
                    arg(stun::cc::methods::toString(method)).
                    arg(SystemError::toString(code)), cl_logDEBUG1);
                return completionHandler(ResultCode::networkError);
            }

            if (const auto error = nx::stun::AsyncClient::hasError(code, message))
            {
                ResultCode resultCode = 
                    ResultCode::otherLogicError;
                if (const auto err = message.getAttribute< nx::stun::attrs::ErrorDescription >())
                    resultCode = fromStunErrorToResultCode(*err);

                NX_LOGX(*error, cl_logDEBUG1);
                //TODO #ak get detailed error from response
                return completionHandler(resultCode);
            }

            completionHandler(ResultCode::ok);
        });
    }
};

} // namespace api
} // namespace hpm
} // namespace nx
