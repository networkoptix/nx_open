/**********************************************************
* 28 oct 2013
* akolesnikov@networkoptix.com
***********************************************************/

#include "ip_range_checker_async.h"

#include <utils/common/log.h>

#include "socket.h"


static const int MAX_HOSTS_CHECKED_SIMULTANEOUSLY = 256;

QnIpRangeCheckerAsync::QnIpRangeCheckerAsync()
:
    m_terminated( false ),
    m_portToScan( 0 ),
    m_startIpv4( 0 ),
    m_endIpv4( 0 ),
    m_nextIPToCheck( 0 )
{
}

QnIpRangeCheckerAsync::~QnIpRangeCheckerAsync()
{
    pleaseStop();
    join();
}

void QnIpRangeCheckerAsync::pleaseStop()
{
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );
        m_terminated = true;
    }
}

void QnIpRangeCheckerAsync::join()
{
    //waiting for scan to finish
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );
    while( !m_socketsBeingScanned.empty() )
        m_cond.wait( lk.mutex() );
}

QStringList QnIpRangeCheckerAsync::onlineHosts( const QHostAddress& startAddr, const QHostAddress& endAddr, int portToScan )
{
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );
        m_openedIPs.clear();

        m_portToScan = portToScan;
        m_startIpv4 = startAddr.toIPv4Address();
        m_endIpv4 = endAddr.toIPv4Address();

        if( m_endIpv4 < m_startIpv4 )
            return QList<QString>();

        m_nextIPToCheck = m_startIpv4;
        for( int i = 0; i < MAX_HOSTS_CHECKED_SIMULTANEOUSLY; ++i )
        {
            if( !launchHostCheck() )
                break;
        }
    }

    join();

    return m_openedIPs;
}

size_t QnIpRangeCheckerAsync::hostsChecked() const
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );
    return (m_nextIPToCheck - m_startIpv4) - m_socketsBeingScanned.size();
}

int QnIpRangeCheckerAsync::maxHostsCheckedSimultaneously()
{
    return MAX_HOSTS_CHECKED_SIMULTANEOUSLY;
}

bool QnIpRangeCheckerAsync::launchHostCheck()
{
    if( m_terminated )
        return false;

    if( m_nextIPToCheck > m_endIpv4 )
        return false;  //all ip addresses are being scanned at the moment
    quint32 ipToCheck = m_nextIPToCheck++;

    nx_http::AsyncHttpClientPtr httpClient = std::make_shared<nx_http::AsyncHttpClient>();
    connect(
        httpClient.get(), &nx_http::AsyncHttpClient::responseReceived,
        this, &QnIpRangeCheckerAsync::onDone,
        Qt::DirectConnection );
    connect(
        httpClient.get(), &nx_http::AsyncHttpClient::done,
        this, &QnIpRangeCheckerAsync::onDone,
        Qt::DirectConnection );
    if( !httpClient->doGet( QUrl( lit("http://%1:%2/").arg(QHostAddress(ipToCheck).toString()).arg(m_portToScan) ) ) )
        return true;
 
    m_socketsBeingScanned.insert( httpClient );

    return true;
}

void QnIpRangeCheckerAsync::onDone( nx_http::AsyncHttpClientPtr httpClient )
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );

    std::set<nx_http::AsyncHttpClientPtr>::iterator it = m_socketsBeingScanned.find( httpClient );
    Q_ASSERT( it != m_socketsBeingScanned.end() );
    if( httpClient->totalBytesRead() > 0 )
        m_openedIPs.push_back( httpClient->url().host() );

    httpClient->terminate();
    m_socketsBeingScanned.erase( it );

    launchHostCheck();

    if( m_socketsBeingScanned.empty() )
        m_cond.wakeAll();
}
