// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <api/http_client_pool.h>
#include <api/parsing_utils.h>
#include <api/server_rest_connection_fwd.h>
#include <nx/network/rest/params.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/utils/log/log.h>

namespace nx::vms::client::core {

/**
 * Generic VMSDB connection interface. Like rest::ServerConnection, but without legacy.
 */
class NX_VMS_CLIENT_CORE_API VmsDbConnection
{
public:
    template<typename Model>
    using Callback = std::function<void(rest::Handle, rest::ErrorOrData<Model>)>;

public:
    VmsDbConnection();
    ~VmsDbConnection();

    template<typename Model>
    rest::Handle get(
        const QString& path,
        const nx::network::rest::Params& params,
        Callback<Model> callback,
        nx::utils::AsyncHandlerExecutor executor)
    {
        auto parseCallback =
            [callback = executor.bind(callback)](ContextPtr context) mutable
            {
                const auto& response = context->response;
                bool success = context->systemError == SystemError::noError && context->hasResponse();
                auto result = rest::parseMessageBody<rest::ErrorOrData<Model>>(
                    Qn::SerializationFormat::json,
                    response.messageBody.toRawByteArray(),
                    response.statusLine,
                    &success);

                // TODO: #amalov Check for network error.

                callback(context->handle, std::move(result));
            };

        return request(nx::network::http::Method::get, path, params, std::move(parseCallback));
    }

private:
    using ContextPtr = nx::network::http::ClientPool::ContextPtr;

    rest::Handle request(
        nx::network::http::Method method,
        const QString& path,
        const nx::network::rest::Params& params,
        std::function<void (ContextPtr context)>&& callback);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
