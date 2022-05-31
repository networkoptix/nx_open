// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <memory>

#include <QtCore/QTimer>

#include <nx/cloud/db/api/connection.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>

namespace nx::vms::client::core {

class CloudConnectionFactory;

 /**
  * Encapsulates connection, timer and time constants for refreshing cloud access tokens.
  *
  * Periodically checks token expiration time supplied using onTokenUpdated().
  * Calls sessionTokenExpiring signal when token needs refreshment.
  * The parent should respond to signal and issue new token using provided methods.
  */
class CloudSessionTokenUpdater: public QObject, public RemoteConnectionAware
{
    Q_OBJECT

public:
    CloudSessionTokenUpdater(QObject* parent);
    virtual ~CloudSessionTokenUpdater() override;

    using IssueTokenHandler = std::function<void(
        nx::cloud::db::api::ResultCode,
        nx::cloud::db::api::IssueTokenResponse)>;

    void issueToken(
        const nx::cloud::db::api::IssueTokenRequest& request,
        IssueTokenHandler handler,
        nx::utils::AsyncHandlerExecutor executor = {});

    // Should be called when new access token is received.
    void onTokenUpdated(std::chrono::microseconds expirationTime);

signals:
    void sessionTokenExpiring();

private:
    void onTimer();

private:
    std::unique_ptr<CloudConnectionFactory> m_cloudConnectionFactory;
    std::unique_ptr<nx::cloud::db::api::Connection> m_cloudConnection;
    QTimer* m_timer = nullptr;
    std::chrono::microseconds m_expirationTime = std::chrono::microseconds::zero();
};

} // namespace nx::vms::client::core
