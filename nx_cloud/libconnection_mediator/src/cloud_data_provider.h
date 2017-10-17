#pragma once

#include <boost/optional.hpp>

#include <nx/utils/timer_manager.h>
#include <nx/network/buffer.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/url.h>

#include <nx/cloud/cdb/api/connection.h>

namespace nx {
namespace hpm {

/**
 * Cloud DB data interface.
 */
class AbstractCloudDataProvider
{
public:
    virtual ~AbstractCloudDataProvider() = 0;

    struct System
    {
        String id;
        String authKey;
        bool mediatorEnabled;

        System();
        System(
            String authKey_,
            bool mediatorEnabled_ = false);
        System(
            String id_,
            String authKey_,
            bool mediatorEnabled_);
    };

    virtual boost::optional< System > getSystem(const String& systemId) const = 0;
};

// for GMock only
std::ostream& operator<<(
    std::ostream& os,
    const boost::optional< AbstractCloudDataProvider::System >& system);

class AbstractCloudDataProviderFactory
{
public:
    typedef std::function<
        std::unique_ptr<AbstractCloudDataProvider>(
            const boost::optional<nx::utils::Url>& cdbUrl,
            const std::string& user,
            const std::string& password,
            std::chrono::milliseconds updateInterval,
            std::chrono::milliseconds startTimeout)> FactoryFunc;

    virtual ~AbstractCloudDataProviderFactory() {}

    static std::unique_ptr<AbstractCloudDataProvider> create(const boost::optional<nx::utils::Url> &cdbUrl,
        const std::string& user,
        const std::string& password,
        std::chrono::milliseconds updateInterval,
        std::chrono::milliseconds startTimeout);

    /**
     * @return Initial factory func.
     */
    static FactoryFunc setFactoryFunc(FactoryFunc factoryFunc);
};

/**
 * Cloud DB data interface over nx::cdb::api::ConnectionFactory.
 */
class CloudDataProvider:
    public AbstractCloudDataProvider
{
public:
    static const std::chrono::milliseconds DEFAULT_UPDATE_INTERVAL;

    CloudDataProvider(
        const boost::optional<nx::utils::Url>& cdbUrl,
        const std::string& user,
        const std::string& password,
        std::chrono::milliseconds updateInterval = DEFAULT_UPDATE_INTERVAL,
        std::chrono::milliseconds startTimeout = std::chrono::milliseconds::zero());
    ~CloudDataProvider();

    virtual boost::optional< System > getSystem(const String& systemId) const override;

private:
    const std::chrono::milliseconds m_updateInterval;
    const std::chrono::steady_clock::time_point m_startTime;
    const std::chrono::milliseconds m_startTimeout;

    void updateSystemsAsync();

    mutable QnMutex m_mutex;
    std::map< String, System > m_systemCache;

    bool m_isTerminated;
    nx::utils::TimerManager::TimerGuard m_timerGuard;

    std::unique_ptr< cdb::api::ConnectionFactory > m_connectionFactory;
    std::unique_ptr< cdb::api::Connection > m_connection;
};

} // namespace hpm
} // namespace nx
