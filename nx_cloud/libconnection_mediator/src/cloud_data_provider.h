#ifndef CLOUD_DATA_PROVIDER_H
#define CLOUD_DATA_PROVIDER_H

#include <utils/common/timermanager.h>
#include <utils/network/buffer.h>
#include <utils/thread/mutex.h>
#include <cdb/connection.h>

namespace nx {
namespace hpm {

//! Cloud DB data interface
class CloudDataProviderBase
{
public:
    virtual ~CloudDataProviderBase() = 0;

    struct System
    {
        String authKey;
        bool mediatorEnabled;

        System( String authKey_, bool mediatorEnabled_ = false );
    };

    virtual boost::optional< System > getSystem( const String& systemId ) const = 0;
};

// for GMock only
std::ostream& operator<<( std::ostream& os,
                          const boost::optional< CloudDataProviderBase::System >& system );

//! Cloud DB data interface over \class nx::cdb::api::ConnectionFactory
class CloudDataProvider
    : public CloudDataProviderBase
{
public:
    static const TimerDuration DEFAULT_UPDATE_INTERVAL;

    CloudDataProvider( const std::string& address,
                       const std::string& user, const std::string& password,
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
