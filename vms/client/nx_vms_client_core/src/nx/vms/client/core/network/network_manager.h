// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>

#include <nx/network/http/http_types.h>
#include <nx/string.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/url.h>

namespace ec2 { enum class ErrorCode; }
namespace nx::network::http { class AsyncClient; }

namespace nx::vms::client::core {

/**
 * Lightweight class for sending client-server http requests. Actually represents a proxy over
 * nx::network::http::AsyncClient list with the integration to the Qt signal-slot system. Allows to
 * receive callbacks either in the AIO thread or in the caller thread (by default).
 * Class is not thread-safe, should be used from the main thread only.
 * Part of the logic is tightly bound to the our server API (e.g. passing error codes in headers),
 * so for now class shouldn't be treated as a generic purpose one.
 */
class NX_VMS_CLIENT_CORE_API NetworkManager: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    // TODO: #sivanov Move out server-dependent logic and use http::Response instead.
    struct Response: nx::network::http::Response
    {
        ec2::ErrorCode errorCode;
        nx::String contentType;
    };

    NetworkManager(QObject* parent = nullptr);
    virtual ~NetworkManager() override;

    void pleaseStopSync();

    /**
     * Setup some reasonable request timeouts. Recommended to call this method if providing manually
     * constructed AsyncClient.
     */
    static void setDefaultTimeouts(nx::network::http::AsyncClient* request);

    using RequestCallback = nx::utils::MoveOnlyFunc<void(Response)>;

    /**
     * Send request of the provided type to the following url. Callback will be delivered using Qt
     * signal-slot system to the request owner.
     */
    void doRequest(
        nx::network::http::Method method,
        std::unique_ptr<nx::network::http::AsyncClient> request,
        nx::utils::Url url,
        QObject* owner,
        RequestCallback callback,
        Qt::ConnectionType connectionType = Qt::AutoConnection);

    /**
     * Send GET request to the following url. Callback will be delivered using Qt signal-slot
     * system to the request owner.
     */
    void doGet(
        std::unique_ptr<nx::network::http::AsyncClient> request,
        nx::utils::Url url,
        QObject* owner,
        RequestCallback callback,
        Qt::ConnectionType connectionType = Qt::AutoConnection);

    /**
     * Send POST request to the following url. Callback will be delivered using Qt signal-slot
     * system to the request owner.
     */
    void doPost(
        std::unique_ptr<nx::network::http::AsyncClient> request,
        nx::utils::Url url,
        QObject* owner,
        RequestCallback callback,
        Qt::ConnectionType connectionType = Qt::AutoConnection);

    /**
     * Send PUT request to the following url. Callback will be delivered using Qt signal-slot
     * system to the request owner.
     */
    void doPut(
        std::unique_ptr<nx::network::http::AsyncClient> request,
        nx::utils::Url url,
        QObject* owner,
        RequestCallback callback,
        Qt::ConnectionType connectionType = Qt::AutoConnection);

    /**
     * Send DELETE request to the following url. Callback will be delivered using Qt signal-slot
     * system to the request owner.
     */
    void doDelete(
        std::unique_ptr<nx::network::http::AsyncClient> request,
        nx::utils::Url url,
        QObject* owner,
        RequestCallback callback,
        Qt::ConnectionType connectionType = Qt::AutoConnection);

    using ConnectCallback =
        std::function<void(std::shared_ptr<nx::network::http::AsyncClient> client)>;

    /**
    * Send CONNECT request to proxy with target address in request line.
    * Connected socket can be extracted and used in new AsyncClient for proxified request.
    */
    void doConnect(std::unique_ptr<nx::network::http::AsyncClient> client,
        const nx::utils::Url& proxyUrl,
        const std::string& targetHost,
        QObject* owner,
        ConnectCallback callback,
        Qt::ConnectionType connectionType = Qt::AutoConnection);

signals:
    void onRequestDone(
        int requestId,
        nx::vms::client::core::NetworkManager::Response response,
        QPrivateSignal);

    void onConnectDone(int requestId,
        std::shared_ptr<nx::network::http::AsyncClient> client,
        QPrivateSignal);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
