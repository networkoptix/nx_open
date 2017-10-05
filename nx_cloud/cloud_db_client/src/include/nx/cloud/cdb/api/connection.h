#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "account_manager.h"
#include "auth_provider.h"
#include "maintenance_manager.h"
#include "module_info.h"
#include "result_code.h"
#include "system_manager.h"

namespace nx {
namespace cdb {
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
namespace api {

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

    /**
     * Set credentials to use.
     * This method does not try to connect to cloud_db check credentials.
     */
    virtual void setCredentials(
        const std::string& login,
        const std::string& password) = 0;
    virtual void setProxyCredentials(
        const std::string& login,
        const std::string& password) = 0;
    virtual void setProxyVia(
        const std::string& proxyHost,
        std::uint16_t proxyPort) = 0;
};

class Connection:
    public BaseConnection
{
public:
    virtual ~Connection() {}

    virtual void setRequestTimeout(std::chrono::milliseconds) = 0;
    virtual std::chrono::milliseconds requestTimeout() const = 0;

    virtual api::AccountManager* accountManager() = 0;
    virtual api::SystemManager* systemManager() = 0;
    virtual api::AuthProvider* authProvider() = 0;
    /**
     * Maintenance manager is for accessing cloud internal data for maintenance/debug purposes.
     * NOTE: Available only in debug environment.
     */
    virtual api::MaintenanceManager* maintenanceManager() = 0;

    /** Pings cloud_db with current creentials. */
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
        const std::string& username,
        const std::string& password) = 0;
    virtual std::unique_ptr<api::EventConnection> createEventConnection() = 0;
    virtual std::unique_ptr<api::EventConnection> createEventConnection(
        const std::string& username,
        const std::string& password) = 0;
    /** Returns text description of resultCode. */
    virtual std::string toString(api::ResultCode resultCode) const = 0;

    /**
     * Explicitely specify endpoint of cloud module. If this method not called, endpoint is detected automatically.
     * NOTE: Call this method only if you are sure about what you are doing.
     */
    virtual void setCloudUrl(const std::string& url) = 0;
};

} // namespace api
} // namespace cdb
} // namespace nx

extern "C"
{
    nx::cdb::api::ConnectionFactory* createConnectionFactory();
    void destroyConnectionFactory(nx::cdb::api::ConnectionFactory* factory);
}
