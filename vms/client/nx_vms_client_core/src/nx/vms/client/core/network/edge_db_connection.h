// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <optional>

#include <api/http_client_pool.h>
#include <api/parsing_utils.h>
#include <api/server_rest_connection_fwd.h>
#include <nx/network/rest/params.h>
#include <nx/reflect/json.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/utils/log/log.h>
#include <nx/utils/system_error.h>

namespace nx::vms::client::core {

/**
 * Generic edgeDb connection interface. Like rest::ServerConnection, but without legacy.
 */
class NX_VMS_CLIENT_CORE_API EdgeDbConnection
{
public:
    template<typename Model>
    using Callback = std::function<void(rest::Handle, rest::ErrorOrData<Model>)>;

    /** Receives the network error or the response, or neither if the request is canceled. */
    using RawCallback = std::function<void(
        rest::Handle, SystemError::ErrorCode, std::optional<nx::network::http::Response>)>;

public:
    EdgeDbConnection();
    virtual ~EdgeDbConnection();

    /**
     * Sends GET request and parses the response as `Model`. The callback is called through the
     * executor. Returns 0 if the request is not sent, e.g. when the Cloud is not online, and the
     * callback is not called then, like in `rest::ServerConnection`.
     */
    template<typename Model>
    rest::Handle get(const QString& path,
        const nx::network::rest::Params& params,
        Callback<Model> callback,
        nx::utils::AsyncHandlerExecutor executor)
    {
        auto parseCallback = [callback = executor.bind(std::move(callback))](rest::Handle handle,
                                 SystemError::ErrorCode systemError,
                                 std::optional<nx::network::http::Response> response) mutable
        {
            callback(handle, parseResponse<Model>(systemError, response));
        };

        return sendGet(path, params, std::move(parseCallback));
    }

    /** Cancels the request. Its callback is called with an error or not called at all. */
    virtual void cancel(rest::Handle handle);

protected:
    /** Sends GET request to the Cloud, returns 0 if it is not sent. Overridden in tests. */
    virtual rest::Handle sendGet(
        const QString& path, const nx::network::rest::Params& params, RawCallback callback);

private:
    template<typename Model>
    static rest::ErrorOrData<Model> parseResponse(SystemError::ErrorCode systemError,
        const std::optional<nx::network::http::Response>& response)
    {
        using nx::network::rest::Result;

        if (systemError != SystemError::noError)
        {
            return std::unexpected(Result::serviceUnavailable(
                QString::fromStdString(SystemError::toString(systemError))));
        }

        if (!response)
            return std::unexpected(Result::serviceUnavailable());

        if (!nx::network::http::StatusCode::isSuccessCode(response->statusLine.statusCode))
            return std::unexpected(parseError(*response));

        bool success = false;
        return rest::parseMessageBody<rest::ErrorOrData<Model>>(Qn::SerializationFormat::json,
            response->messageBody.toRawByteArray(),
            response->statusLine,
            &success);
    }

    /**
     * Reads the edgeDb error. `rest::parseMessageBody()` can't be used for it, as it requires the
     * legacy numeric error code, which edgeDb does not send.
     */
    static nx::network::rest::Result parseError(const nx::network::http::Response& response);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
