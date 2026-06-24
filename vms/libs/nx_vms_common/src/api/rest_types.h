// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <expected>

#include <QtCore/QString>

#include <nx/network/http/http_types.h>
#include <nx/network/rest/result.h>

#include "server_rest_connection_fwd.h"

namespace rest {

enum class Reason
{
    ok,
    fail, //< Real failure (server error, transport error, etc).
    cancel //< User dismissed the client-side re-auth dialog.
};

/**
 * Implicitly convertible to/from bool, which can be treated as an operation success/fail status.
 * Reason allows distinguishing user cancellation from an unexpected failure.
 */
class Status
{
public:
    Status() = default;
    explicit Status(Reason reason) : m_reason(reason) {}
    Status(bool success) : m_reason(success ? Reason::ok : Reason::fail) {}

    operator bool() const { return m_reason == Reason::ok; }

    friend bool operator==(const Status& lhs, const Status& rhs)
    {
        return lhs.m_reason == rhs.m_reason;
    }

    Reason reason() const { return m_reason; }

private:
    Reason m_reason = Reason::fail;
};

struct EmptyResponseType {};

template<typename Data>
using ErrorOrData = std::expected<Data, nx::network::rest::Result>;

using ErrorOrEmpty = ErrorOrData<EmptyResponseType>;

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
