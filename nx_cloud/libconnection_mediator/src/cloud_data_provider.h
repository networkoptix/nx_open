#ifndef CLOUD_DATA_PROVIDER_H
#define CLOUD_DATA_PROVIDER_H

#include <cdb/connection.h>
#include <nx/utils/timer_manager.h>
#include <nx/network/buffer.h>
#include <nx/utils/thread/mutex.h>


namespace nx {
namespace hpm {

//! Cloud DB data interface
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

    virtual boost::optional< System > getSystem( const String& systemId ) const = 0;
};

// for GMock only
std::ostream& operator<<( std::ostream& os,
                          const boost::optional< AbstractCloudDataProvider::System >& system );

class AbstractCloudDataProviderFactory
{
public:
    typedef std::function<
        std::unique_ptr<AbstractCloudDataProvider>(
            const std::string& address,
            const std::string& user,
            const std::string& password,
            TimerDuration updateInterval)> FactoryFunc;

    virtual ~AbstractCloudDataProviderFactory() {}

    static std::unique_ptr<AbstractCloudDataProvider> create(
        const std::string& address,
        const std::string& user,
        const std::string& password,
        TimerDuration updateInterval);

    static void setFactoryFunc(FactoryFunc factoryFunc);
};

//! Cloud DB data interface over \class nx::cdb::api::ConnectionFactory
class CloudDataProvider
    : public AbstractCloudDataProvider
{
public:
    static const TimerDuration DEFAULT_UPDATE_INTERVAL;

    CloudDataProvider( const std::string& address,
                       const std::string& user,
                       const std::string& password,
                       TimerDuration updateInterval = DEFAULT_UPDATE_INTERVAL );
    ~CloudDataProvider();

    virtual boost::optional< System > getSystem( const String& systemId ) const override;

private:
    const TimerDuration m_updateInterval;

    void updateSystemsAsync();

    mutable QnMutex m_mutex;
    std::map< String, System > m_systemCache;

    bool m_isTerminated;
    TimerManager::TimerGuard m_timerGuard;

    std::unique_ptr< cdb::api::ConnectionFactory > m_connectionFactory;
    std::unique_ptr< cdb::api::Connection > m_connection;
};

} // namespace hpm
} // namespace nx

#endif // CLOUD_DATA_PROVIDER_H
