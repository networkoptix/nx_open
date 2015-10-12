#include "cloud_data_provider.h"

#include <utils/common/log.h>
#include <utils/common/log_message.h>

static const std::chrono::seconds UPDATE_INTERVAL( 10 );

namespace nx {
namespace hpm {

CloudDataProviderIf::~CloudDataProviderIf()
{
}


CloudDataProviderIf::System::System( String authKey_, bool mediatorEnabled_ )
    : authKey( authKey_ )
    , mediatorEnabled( mediatorEnabled_ )
{
}

std::ostream& operator<<( std::ostream& os,
                          const boost::optional< CloudDataProviderIf::System >& system )
{
    if( !system )
        return os << "boost::none";

    return os << "System(key=" << system->authKey.data()
              << ", mediatorEnabled=" << system->mediatorEnabled << ")";
}

// impl

CloudDataProvider::CloudDataProvider( const std::string& user,
                                    const std::string& password )
    : m_isTerminated( false )
    , m_connectionFactory( createConnectionFactory() )
    , m_connection( m_connectionFactory->createConnection( user, password ) )
{
    updateSystemsAsync();
}

CloudDataProvider::~CloudDataProvider()
{
    QnMutexLocker lk( &m_mutex );
    m_isTerminated = true;
}

boost::optional< CloudDataProviderIf::System >
    CloudDataProvider::getSystem( const String& systemId ) const
{
    QnMutexLocker lk( &m_mutex );
    const auto it = m_systemCache.find( systemId );
    if( it == m_systemCache.end() )
        return boost::none;

    return System( String( it->second.authKey.c_str() ),
                   it->second.cloudConnectionSubscriptionStatus );
}

void CloudDataProvider::updateSystemsAsync()
{
    {
        QnMutexLocker lk( &m_mutex );
        if( m_isTerminated )
            return;
    }

    m_connection->systemManager()->getSystems(
                [ this ]( cdb::api::ResultCode code, cdb::api::SystemDataList systems )
    {
        if( code != cdb::api::ResultCode::ok )
        {
            NX_LOGX( lm("Error: %1")
                     .arg( m_connectionFactory->toString( code ) ), cl_logERROR );

            // TODO: shell we m_systemCache.clear() after a few failing attempts?
        }
        else
        {
            NX_LOGX( lm("There are %1 systems updated")
                     .arg( systems.systems.size() ), cl_logINFO );

            QnMutexLocker lk( &m_mutex );
            m_systemCache.clear();
            for( auto& sys : systems.systems )
                m_systemCache.emplace( sys.id.toSimpleString().toUtf8(),
                                       std::move( sys ) );
        }

        m_timerGuard = TimerManager::instance()->addTimer(
            [ this ]( quint64 timerID )
        {
            Q_ASSERT_X( timerID == m_timerGuard.get(), Q_FUNC_INFO, "Wrong timer" );
            updateSystemsAsync();
        },
        UPDATE_INTERVAL );
    } );
}

} // namespace hpm
} // namespace nx
