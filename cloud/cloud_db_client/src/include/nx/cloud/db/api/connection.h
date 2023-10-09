// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include <nx/network/http/auth_tools.h>
#include <nx/network/socket_factory.h>

#include "account_manager.h"
#include "auth_provider.h"
#include "batch_user_processing_manager.h"
#include "maintenance_manager.h"
#include "module_info.h"
#include "oauth_manager.h"
#include "organization_manager.h"
#include "result_code.h"
#include "system_manager.h"
#include "two_factor_auth_manager.h"

namespace nx::network::aio { class AbstractAioThread; }

/**
 * Contains classes and methods for manipulating cloud_db data.
 * Many methods accept completionHandler a an argument.
 * This is functor that is called on operation completion/failure
 * and reports ResultCode and output data (if applicable).
 * WARNING: Implementation is allowed to invoke completionHandler directly within calling function.
 *   It allows all these methods to return void.
 *
 * Generally, some methods can be forbidden for credentials.
 * E.g., if credentials are system credentials, api::AccountManager::getAccount is forbidden
 */
namespace nx::cloud::db::api {

class ConnectionLostEvent
{
};

class SystemAccessListModifiedEvent
{
};

class BaseConnection
{
public:
    virtual ~BaseConnection() {}

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) = 0;

    /** Set credentials to be used in every subsequent request. */
    virtual void setCredentials(nx::network::http::Credentials credentials) = 0;

    virtual void setProxyVia(
        const std::string& proxyHost,
        std::uint16_t proxyPort,
        nx::network::http::Credentials credentials,
        nx::network::ssl::AdapterFunc adapterFunc) = 0;
};

class Connection:
    public BaseConnection
{
public:
    virtual ~Connection() {}

    virtual void setRequestTimeout(std::chrono::milliseconds) = 0;
    virtual std::chrono::milliseconds requestTimeout() const = 0;

    /** Set headers to be added to every subsequent request. */
    virtual void setAdditionalHeaders(nx::network::http::HttpHeaders headers) = 0;

    virtual api::AccountManager* accountManager() = 0;
    virtual api::SystemManager* systemManager() = 0;
    virtual api::OrganizationManager* organizationManager() = 0;
    virtual api::AuthProvider* authProvider() = 0;

    /**
     * Maintenance manager is for accessing cloud internal data for maintenance/debug purposes.
     * NOTE: Available only in debug environment.
     */
    virtual api::MaintenanceManager* maintenanceManager() = 0;

    virtual api::OauthManager* oauthManager() = 0;
    virtual api::TwoFactorAuthManager* twoFactorAuthManager() = 0;
    virtual api::BatchUserProcessingManager* batchUserProcessingManager() = 0;

    /** Pings cloud_db with current credentials. */
    virtual void ping(std::function<void(api::ResultCode, api::ModuleInfo)> completionHandler) = 0;
};

class SystemEventHandlers
{
public:
    std::function<void(ConnectionLostEvent)> onConnectionLost;
    std::function<void(SystemAccessListModifiedEvent)> onSystemAccessListUpdated;
};

/**
 * If existing connection has failed, reconnect attempt will be performed.
 * If failed to reconnect, onConnectionLost event will be reported.
 */
class EventConnection:
    public BaseConnection
{
public:
    /** If event handler is running in another thread, blocks until handler has returned. */
    virtual ~EventConnection() {}

    /**
     * Must be called just after object creation to start receiving events.
     * @param eventHandlers Handles are invoked in unspecified internal thread.
     *      Single EventConnection instance always uses same thread.
     * @param completionHandler Used to report result. Can be nullptr
     * NOTE: This method can be called again only after onConnectionLost has been reported.
     */
    virtual void start(
        SystemEventHandlers eventHandlers,
        std::function<void (ResultCode)> completionHandler) = 0;
};

class ConnectionFactory
{
public:
    virtual ~ConnectionFactory() {}

    /**
     * Connects to cloud_db to check user credentials.
     * NOTE: Instantiates connection only if connection to cloud is available and provided credentials are valid.
     */
    virtual void connect(
        std::function<void(api::ResultCode, std::unique_ptr<api::Connection>)> completionHandler) = 0;

    /**
     * Creates connection object without checking credentials provided.
     * NOTE: No connection to cloud is performed in this method!
     */
    virtual std::unique_ptr<api::Connection> createConnection() = 0;

    virtual std::unique_ptr<api::Connection> createConnection(
        nx::network::http::Credentials credentials) = 0;

    /**
     * @return Text description of resultCode.
     */
    virtual std::string toString(api::ResultCode resultCode) const = 0;

    /**
     * Explicitly specify endpoint of cloud module. If this method not called, endpoint is detected automatically.
     * NOTE: Call this method only if you are sure about what you are doing.
     */
    virtual void setCloudUrl(const std::string& url) = 0;
};

} // namespace nx::cloud::db::api

extern "C"
{
    nx::cloud::db::api::ConnectionFactory* createConnectionFactory();
    void destroyConnectionFactory(nx::cloud::db::api::ConnectionFactory* factory);
}
