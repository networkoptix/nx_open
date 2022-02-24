// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <string>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/reflect/string_conversion.h>
#include <nx/network/http/http_types.h>
#include <nx/network/http/server/api_request_result.h>
#include <nx/network/abstract_socket.h>
#include <nx/utils/std/optional.h>

#include "relay_api_result_code.h"

namespace nx::cloud::relay::api {

struct Result
{
    ResultCode code = ResultCode::ok;
    std::string text;

    Result(): Result(ResultCode::ok) {}
    Result(ResultCode code): Result(code, nx::reflect::toString(code)) {}
    Result(ResultCode code, std::string text): code(code), text(std::move(text)) {}

    bool ok() const { return code == ResultCode::ok; }

    bool operator==(const Result& rhs) const
    {
        return std::tie(code, text) == std::tie(rhs.code, rhs.text);
    }
};

#define RelayResult_Fields (code)(text)

NX_REFLECTION_INSTRUMENT(Result, RelayResult_Fields)
QN_FUSION_DECLARE_FUNCTIONS(Result, (json), NX_NETWORK_API)

NX_NETWORK_API void convert(
    const Result& result,
    nx::network::http::ApiRequestResult* httpResult);

//-------------------------------------------------------------------------------------------------

struct NX_NETWORK_API ResultDescriptor
{
    using ResultCode = api::Result;

    template<typename... Output>
    static api::Result getResultCode(
        SystemError::ErrorCode systemErrorCode,
        const network::http::Response* response,
        const network::http::ApiRequestResult& /*fusionRequestResult*/,
        const Output&...)
    {
        if (systemErrorCode != SystemError::noError)
            return systemErrorCodeToResultCode(systemErrorCode);

        if (!response)
            return api::ResultCode::networkError;

        return getResultCodeFromResponse(*response);
    }

private:
    static api::Result systemErrorCodeToResultCode(SystemError::ErrorCode systemErrorCode);
    static api::Result getResultCodeFromResponse(const network::http::Response& response);
};

//-------------------------------------------------------------------------------------------------

struct BeginListeningRequest
{
    std::string peerName;
    /** E.g., 0.1 */
    std::string protocolVersion;
};

#define BeginListeningRequest_Fields (peerName)

NX_REFLECTION_INSTRUMENT(BeginListeningRequest, BeginListeningRequest_Fields)

struct NX_NETWORK_API BeginListeningResponse
{
    int preemptiveConnectionCount;
    std::optional<network::KeepAliveOptions> keepAliveOptions;

    BeginListeningResponse();

    bool operator==(const BeginListeningResponse& right) const;
};

NX_NETWORK_API bool serializeToHeaders(
    nx::network::http::HttpHeaders* where,
    const BeginListeningResponse& what);

NX_NETWORK_API bool deserializeFromHeaders(
    const nx::network::http::HttpHeaders& from,
    BeginListeningResponse* what);

#define BeginListeningResponse_Fields (preemptiveConnectionCount)

NX_REFLECTION_INSTRUMENT(BeginListeningResponse, BeginListeningResponse_Fields)

struct CreateClientSessionRequest
{
    std::string targetPeerName;
    std::string desiredSessionId;

    // This field is used by traffic_relay internally. TODO: #akolesnikov remove it from here.
    bool isRequestReceivedOverSsl = false;
};

#define CreateClientSessionRequest_Fields (targetPeerName)(desiredSessionId)

NX_REFLECTION_INSTRUMENT(CreateClientSessionRequest, CreateClientSessionRequest_Fields)

struct CreateClientSessionResponse
{
    std::string sessionId;
    std::chrono::seconds sessionTimeout;
    /**
     * This url can be different from the initial one in case of redirect to another relay instance.
     */
    std::string actualRelayUrl;
};

#define CreateClientSessionResponse_Fields (sessionId)(sessionTimeout)

NX_REFLECTION_INSTRUMENT(CreateClientSessionResponse, CreateClientSessionResponse_Fields)

NX_NETWORK_API bool serializeToHeaders(
    nx::network::http::HttpHeaders* where,
    const CreateClientSessionResponse& what);

struct ConnectToPeerRequest
{
    std::string sessionId;
};

#define ConnectToPeerRequest_Fields (sessionId)

NX_REFLECTION_INSTRUMENT(ConnectToPeerRequest, ConnectToPeerRequest_Fields)

QN_FUSION_DECLARE_FUNCTIONS(BeginListeningRequest, (json), NX_NETWORK_API)
QN_FUSION_DECLARE_FUNCTIONS(BeginListeningResponse, (json), NX_NETWORK_API)
QN_FUSION_DECLARE_FUNCTIONS(CreateClientSessionRequest, (json), NX_NETWORK_API)
QN_FUSION_DECLARE_FUNCTIONS(CreateClientSessionResponse, (json), NX_NETWORK_API)
QN_FUSION_DECLARE_FUNCTIONS(ConnectToPeerRequest, (json), NX_NETWORK_API)

//-------------------------------------------------------------------------------------------------

struct Relay
{
    int listeningPeerCount = 0;

    bool operator==(const Relay& rhs) const { return listeningPeerCount == rhs.listeningPeerCount; }
};

#define Relay_Fields (listeningPeerCount)

NX_REFLECTION_INSTRUMENT(Relay, Relay_Fields)
QN_FUSION_DECLARE_FUNCTIONS(Relay, (json), NX_NETWORK_API)

using Relays = std::map<std::string /*relay name*/, Relay>;

} // namespace nx::cloud::relay::api
