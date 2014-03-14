
#include "ip_range_checker_async.h"

#include <QMutexLocker>

#include <utils/common/log.h>

#include "socket.h"


static const int PORT_TO_SCAN = 80;

QnIpRangeCheckerAsync::QnIpRangeCheckerAsync()
:
    m_portToScan( 0 ),
    m_endIpv4( 0 ),
    m_nextIPToCheck( 0 )
{
}

QnIpRangeCheckerAsync::~QnIpRangeCheckerAsync()
{
    pleaseStop();
    waitForScanToFinish();
}

void QnIpRangeCheckerAsync::pleaseStop()
{
    //TODO/IMPL
}

//static const int SOCKET_CONNECT_TIMEOUT_MILLIS = 2000;
static const int MAX_HOSTS_CHECKED_SIMULTANEOUSLY = 256;

QStringList QnIpRangeCheckerAsync::onlineHosts( const QHostAddress& startAddr, const QHostAddress& endAddr, int portToScan )
{
    m_openedIPs.clear();

    m_portToScan = portToScan;
    const quint32 startIpv4 = startAddr.toIPv4Address();
    m_endIpv4 = endAddr.toIPv4Address();

    if( m_endIpv4 < startIpv4 )
        return QList<QString>();

    //for( quint32 ip = startIpv4; ip <= endIpv4; ++ip )
    m_nextIPToCheck = startIpv4;
    for( int i = 0; i < MAX_HOSTS_CHECKED_SIMULTANEOUSLY; ++i )
    {
        QMutexLocker lk( &m_mutex );
        if( !launchHostCheck() )
            break;
    }

    waitForScanToFinish();

    return m_openedIPs;
}

//void QnIpRangeCheckerAsync::eventTriggered( Socket* sock, PollSet::EventType eventType ) throw()
//{
//    QMutexLocker lk( &m_mutex );
//
//    if( eventType == PollSet::etWrite )
//        m_openedIPs.push_back( static_cast<CommunicatingSocket*>(sock)->getForeignAddress() );
//
//    std::map<Socket*, QSharedPointer<Socket> >::iterator it = m_socketsBeingScanned.find( sock );
//    assert( it != m_socketsBeingScanned.end() );
//    aio::AIOService::instance()->removeFromWatch( it->second, PollSet::etWrite );
//    m_socketsBeingScanned.erase( it );
//
//    if( m_socketsBeingScanned.empty() )
//        m_cond.wakeAll();
//}

bool QnIpRangeCheckerAsync::launchHostCheck()
{
    quint32 ipToCheck = m_nextIPToCheck++;
    if( ipToCheck > m_endIpv4 )
        return false;  //all ip addresses are being scanned at the moment

    std::shared_ptr<nx_http::AsyncHttpClient> httpClient = std::make_shared<nx_http::AsyncHttpClient>();
    connect(
        httpClient.get(), SIGNAL(responseReceived( nx_http::AsyncHttpClientPtr )),
        this, SLOT(onDone( nx_http::AsyncHttpClientPtr )),
        Qt::DirectConnection );
    connect(
        httpClient.get(), SIGNAL(done( nx_http::AsyncHttpClientPtr )),
        this, SLOT(onDone( nx_http::AsyncHttpClientPtr )),
        Qt::DirectConnection );
    if( !httpClient->doGet( QUrl( lit("http://%1:%2/").arg(QHostAddress(ipToCheck).toString()).arg(m_portToScan) ) ) )
        return true;
 
    m_socketsBeingScanned.insert( httpClient );

    return true;
}

void QnIpRangeCheckerAsync::waitForScanToFinish()
{
    //waiting for scan to finish
    QMutexLocker lk( &m_mutex );
    while( !m_socketsBeingScanned.empty() )
        m_cond.wait( lk.mutex() );
}

void QnIpRangeCheckerAsync::onDone( nx_http::AsyncHttpClientPtr httpClient )
{
    QMutexLocker lk( &m_mutex );

    std::set<std::shared_ptr<nx_http::AsyncHttpClient> >::iterator it = m_socketsBeingScanned.find( httpClient );
    Q_ASSERT( it != m_socketsBeingScanned.end() );
    if( httpClient->totalBytesRead() > 0 )
        m_openedIPs.push_back( httpClient->url().host() );

    m_socketsBeingScanned.erase( it );

    launchHostCheck();

    if( m_socketsBeingScanned.empty() )
        m_cond.wakeAll();
}
