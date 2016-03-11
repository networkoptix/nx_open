#include "cloud_data_provider.h"

#include <ostream>

#include <nx/utils/log/log.h>
#include <nx/utils/log/log_message.h>
#include <utils/common/cpp14.h>


namespace nx {
namespace hpm {

AbstractCloudDataProvider::~AbstractCloudDataProvider()
{
}


static AbstractCloudDataProviderFactory::FactoryFunc cloudDataProviderFactoryFunc;

std::unique_ptr<AbstractCloudDataProvider> AbstractCloudDataProviderFactory::create(
    const std::string& address,
    const std::string& user,
    const std::string& password,
    TimerDuration updateInterval)
{
    if (cloudDataProviderFactoryFunc)
        return cloudDataProviderFactoryFunc(
            address,
            user,
            password,
            updateInterval);
    else
        return std::make_unique<CloudDataProvider>(
            address,
            user,
            password,
            updateInterval);
}

void AbstractCloudDataProviderFactory::setFactoryFunc(FactoryFunc factoryFunc)
{
    cloudDataProviderFactoryFunc = std::move(factoryFunc);
}


AbstractCloudDataProvider::System::System()
:
    mediatorEnabled(false)
{
}

AbstractCloudDataProvider::System::System( String authKey_, bool mediatorEnabled_ )
    : authKey(std::move(authKey_))
    , mediatorEnabled( mediatorEnabled_ )

{
}

AbstractCloudDataProvider::System::System(
    String id_,
    String authKey_,
    bool mediatorEnabled_)
:
    id(std::move(id_)),
    authKey(std::move(authKey_)),
    mediatorEnabled(mediatorEnabled_)
{
}

std::ostream& operator<<( std::ostream& os,
                          const boost::optional< AbstractCloudDataProvider::System >& system )
{
    if( !system )
        return os << "boost::none";

    return os << "System(key=" << system->authKey.data()
              << ", mediatorEnabled=" << system->mediatorEnabled << ")";
}

// impl

const TimerDuration CloudDataProvider::DEFAULT_UPDATE_INTERVAL
    = std::chrono::minutes( 5 );

static nx::cdb::api::ConnectionFactory* makeConnectionFactory( const std::string& address )
{
    auto factory = createConnectionFactory();
    if( factory && !address.empty() )
    {
        const auto colon = address.find( ":" );
        if( colon != std::string::npos )
        {
            const auto host = address.substr( 0, colon );
            const auto port = std::stoi( address.substr( colon + 1 ) );

            factory->setCloudEndpoint( address, port );
            NX_LOG( lm("nx::cdb::api::ConnectionFactory set address %1:%2")
                    .arg(host).arg(port), cl_logALWAYS );
        }
        else
        {
            NX_LOG( lm("nx::cdb::api::ConnectionFactory can not pase address %1")
                     .arg( address ), cl_logERROR );
        }
    }
    return factory;
}

CloudDataProvider::CloudDataProvider( const std::string& address,
                                      const std::string& user,
                                      const std::string& password,
                                      TimerDuration updateInterval )
    : m_updateInterval( std::move( updateInterval ) )
    , m_isTerminated( false )
    , m_connectionFactory( makeConnectionFactory( address ) )
    , m_connection( m_connectionFactory->createConnection( user, password ) )
{
    updateSystemsAsync();
}

CloudDataProvider::~CloudDataProvider()
{
    QnMutexLocker lk( &m_mutex );
    m_isTerminated = true;

    const auto connection = std::move( m_connection );
    const auto guard = std::move( m_timerGuard );

    lk.unlock();
}

boost::optional< AbstractCloudDataProvider::System >
    CloudDataProvider::getSystem( const String& systemId ) const
{
    QnMutexLocker lk( &m_mutex );
    const auto it = m_systemCache.find( systemId );
    if( it == m_systemCache.end() )
        return boost::none;

    return it->second;
}

static QString traceSystems( const std::map< String, AbstractCloudDataProvider::System >& systems )
{
    QStringList list;
    for( const auto sys : systems )
        list << lm("%1 (%2 %3)").arg( sys.first )
                                .arg( sys.second.authKey )
                                .arg( sys.second.mediatorEnabled );
    return list.join( QLatin1String( ", " ) );
}

void CloudDataProvider::updateSystemsAsync()
{
    m_connection->systemManager()->getSystems(
                [ this ]( cdb::api::ResultCode code, cdb::api::SystemDataExList systems )
    {
        if( code != cdb::api::ResultCode::ok )
        {
            NX_LOGX( lm("Error: %1")
                     .arg( m_connectionFactory->toString( code ) ), cl_logERROR );

            // TODO: shall we m_systemCache.clear() after a few failing attempts?
        }
        else
        {
            QnMutexLocker lk( &m_mutex );
            m_systemCache.clear();
            for( auto& sys : systems.systems )
                m_systemCache.emplace(
                    sys.id.toByteArray(),
                    System( String( sys.authKey.c_str() ),
                                    sys.cloudConnectionSubscriptionStatus ) );

            NX_LOGX( lm("There is(are) %1 system(s) updated")
                     .arg( systems.systems.size() ), cl_logDEBUG1 );
            NX_LOGX( lm("Updated: %1")
                     .arg( traceSystems( m_systemCache ) ), cl_logDEBUG2 );
        }

        QnMutexLocker lk( &m_mutex );
        if( !m_isTerminated )
            m_timerGuard = TimerManager::instance()->addTimer(
                [ this ]( quint64 ) { updateSystemsAsync(); }, m_updateInterval );
    } );
}

} // namespace hpm
} // namespace nx
