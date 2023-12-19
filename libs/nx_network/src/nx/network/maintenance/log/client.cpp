// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client.h"

#include "request_path.h"

namespace nx::network::maintenance::log {

Client::Client(const nx::utils::Url& baseApiUrl):
    base_type(baseApiUrl, ssl::kDefaultCertificateCheck)
{
}

Client::~Client()
{
    pleaseStopSync();
}

void Client::getConfiguration(
    nx::utils::MoveOnlyFunc<void(ResultCode, Loggers)> completionHandler)
{
    base_type::template makeAsyncCall<Loggers>(
        http::Method::get,
        kLoggers,
        {}, // query
        std::move(completionHandler));
}

std::tuple<Client::ResultCode, Loggers> Client::getConfiguration()
{
    return base_type::template makeSyncCall<Loggers>(
        http::Method::get, kLoggers, nx::utils::UrlQuery());
}

} // namespace nx::network::maintenance::log
