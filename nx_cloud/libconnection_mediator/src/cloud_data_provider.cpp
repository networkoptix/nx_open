#include "cloud_data_provider.h"

#include <utils/common/log.h>
#include <utils/common/log_message.h>

static const std::chrono::minutes UPDATE_INTERVAL( 1 );

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

    return it->second;
}

static QString traceSystems( const std::map< String, CloudDataProviderIf::System >& systems )
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
            QnMutexLocker lk( &m_mutex );
            m_systemCache.clear();
            for( auto& sys : systems.systems )
                m_systemCache.emplace(
                    sys.id.toSimpleString().toUtf8(),
                    System( String( sys.authKey.c_str() ),
                                    sys.cloudConnectionSubscriptionStatus ) );

            NX_LOGX( lm("There is(are) %1 system(s) updated")
                     .arg( systems.systems.size() ), cl_logDEBUG1 );
            NX_LOGX( lm("Updated: %1")
                     .arg( traceSystems( m_systemCache ) ), cl_logDEBUG2 );
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
