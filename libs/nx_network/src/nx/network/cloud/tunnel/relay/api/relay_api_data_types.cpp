// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "relay_api_data_types.h"

#include <nx/utils/string.h>

namespace nx::cloud::relay::api {

namespace {

static constexpr char kPreemptiveConnectionCountHeaderName[] =
    "Nx-Relay-Preemptive-Connection-Count";
static constexpr int kPreemptiveConnectionCountDefault = 7;

static constexpr char kTcpKeepAliveHeaderName[] =
    "Nx-Relay-Tcp-Connection-Keep-Alive";

} // namespace

BeginListeningResponse::BeginListeningResponse():
    preemptiveConnectionCount(kPreemptiveConnectionCountDefault)
{
}

bool BeginListeningResponse::operator==(const BeginListeningResponse& right) const
{
    return preemptiveConnectionCount == right.preemptiveConnectionCount
        && keepAliveOptions == right.keepAliveOptions;
}

bool serializeToHeaders(nx::network::http::HttpHeaders* where, const BeginListeningResponse& what)
{
    where->emplace(
        kPreemptiveConnectionCountHeaderName,
        std::to_string(what.preemptiveConnectionCount));

    if (what.keepAliveOptions)
        where->emplace(kTcpKeepAliveHeaderName, what.keepAliveOptions->toString());

    return true;
}

bool serializeToHeaders(
    nx::network::http::HttpHeaders* where,
    const CreateClientSessionResponse& what)
{
    where->emplace("Location", what.actualRelayUrl);
    return true;
}

bool deserializeFromHeaders(const nx::network::http::HttpHeaders& from, BeginListeningResponse* what)
{
    auto it = from.find(kPreemptiveConnectionCountHeaderName);
    if (it != from.end())
        what->preemptiveConnectionCount = nx::utils::stoi(it->second);

    it = from.find(kTcpKeepAliveHeaderName);
    if (it != from.end())
    {
        what->keepAliveOptions =
            network::KeepAliveOptions::fromString(it->second);
    }

    return true;
}

//-------------------------------------------------------------------------------------------------

void convert(const Result& result, nx::network::http::ApiRequestResult* httpResult)
{
    if (result.code == api::ResultCode::ok)
    {
        *httpResult = nx::network::http::ApiRequestResult();
        return;
    }

    httpResult->setErrorClass(nx::network::http::ApiRequestErrorClass::logicError);
    httpResult->setErrorText(result.text);
    httpResult->setResultCode(result.text);
}

//-------------------------------------------------------------------------------------------------

api::Result ResultDescriptor::systemErrorCodeToResultCode(
    SystemError::ErrorCode systemErrorCode)
{
    return api::Result{
        api::ResultCode::networkError,
        SystemError::toString(systemErrorCode)};
}

api::Result ResultDescriptor::getResultCodeFromResponse(
    const network::http::Response& response)
{
    using namespace nx::network::http;

    api::ResultCode code = api::ResultCode::unknownError;
    if (StatusCode::isSuccessCode(response.statusLine.statusCode))
        code = api::ResultCode::ok;
    else if (response.statusLine.statusCode == StatusCode::unauthorized)
        code = api::ResultCode::notAuthorized;
    else if (response.statusLine.statusCode == StatusCode::notFound)
        code = api::ResultCode::notFound;

    return api::Result{code, response.statusLine.reasonPhrase};
}

} // namespace nx::cloud::relay::api
