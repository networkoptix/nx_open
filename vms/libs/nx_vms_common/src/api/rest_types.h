// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/network/http/http_types.h>
#include <nx/network/rest/result.h>
#include <nx/utils/std/expected.h>

#include "server_rest_connection_fwd.h"

namespace rest {

struct EmptyResponseType {};

template<typename Data>
using ErrorOrData = nx::utils::expected<Data, nx::network::rest::Result>;

template <typename T>
struct ResultWithData
{
    ResultWithData() {}
    ResultWithData(const nx::network::rest::Result& restResult, T data):
        error(restResult.errorId),
        errorString(restResult.errorString),
        data(std::move(data))
    {
    }

    ResultWithData(const ResultWithData&) = delete;
    ResultWithData(ResultWithData&&) = default;
    ResultWithData& operator=(const ResultWithData&) = delete;
    ResultWithData& operator=(ResultWithData&&) = default;

    nx::network::rest::ErrorId error = nx::network::rest::ErrorId::ok;
    QString errorString;
    T data;
};

using DataCallback = nx::MoveOnlyFunc<void(
    bool success,
    Handle requestId,
    QByteArray result,
    const nx::network::http::HttpHeaders& headers)>;

} // namespace rest
