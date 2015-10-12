#ifndef CLOUD_DATA_PROVIDER_H
#define CLOUD_DATA_PROVIDER_H

#include <utils/common/timermanager.h>

#include <connection_factory.h>

namespace nx {
namespace hpm {

//! Cloud DB data interface
class CloudDataProviderIf
{
public:
    virtual ~CloudDataProviderIf() = 0;

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
                          const boost::optional< CloudDataProviderIf::System >& system );

//! Cloud DB data interface over \class nx::cdb::api::ConnectionFactory
class CloudDataProvider
    : public CloudDataProviderIf
{
public:
    CloudDataProvider( const std::string& user, const std::string& password );
    ~CloudDataProvider();

    virtual boost::optional< System > getSystem( const String& systemId ) const override;

private:
    void updateSystemsAsync();

    mutable QnMutex m_mutex;
    std::map< String, cdb::api::SystemData > m_systemCache;

    bool m_isTerminated;
    TimerManager::TimerGuard m_timerGuard;

    std::unique_ptr< cdb::api::ConnectionFactory > m_connectionFactory;
    std::unique_ptr< cdb::api::Connection > m_connection;
};

} // namespace hpm
} // namespace nx

#endif // CLOUD_DATA_PROVIDER_H
