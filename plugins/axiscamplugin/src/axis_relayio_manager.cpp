/**********************************************************
* 12 apr 2013
* akolesnikov
***********************************************************/

#include "axis_relayio_manager.h"

#include <algorithm>
#include <cstring>

#include <QtCore/QDateTime>
#include <QtCore/QUrlQuery>

#include "axis_camera_manager.h"
#include "axis_cam_params.h"
#include "axis_camera_plugin.h"
#include "sync_http_client.h"


AxisRelayIOManager::AxisRelayIOManager(
    AxisCameraManager* cameraManager,
    unsigned int inputPortCount,
    unsigned int outputPortCount )
:
    m_refManager( cameraManager->refManager() ),
    m_cameraManager( cameraManager ),
    m_inputPortCount( inputPortCount ),
    m_outputPortCount( outputPortCount ),
    m_multipartedParsingState( waitingDelimiter )
{
    std::unique_ptr<SyncHttpClient> httpClient;
    if( !httpClient.get() )
        httpClient.reset( new SyncHttpClient(
            AxisCameraPlugin::instance()->networkAccessManager(),
            m_cameraManager->cameraInfo().url,
            DEFAULT_AXIS_API_PORT,
            m_cameraManager->credentials() ) );

    //reading port direction and names
    for( unsigned int i = 0; i < m_inputPortCount+m_outputPortCount; ++i )
    {
        QByteArray portDirection;
        int status = AxisCameraManager::readAxisParameter(
            httpClient.get(),
            QString::fromLatin1("IOPort.I%1.Direction").arg(i).toLatin1(),
            &portDirection );
        if( status != SyncHttpClient::HTTP_OK )
            continue;

        QString portName;
        status = AxisCameraManager::readAxisParameter(
            httpClient.get(),
            QString::fromLatin1("IOPort.I%1.%2.Name").arg(i).arg(QString::fromUtf8(portDirection)).toLatin1(),
            &portName );
        if( status != SyncHttpClient::HTTP_OK )
            continue;

        if( portDirection == "input" )
            m_inputPortNameToIndex[portName] = i;
        else if( portDirection == "output" )
            m_outputPortNameToIndex[portName] = i;
    }

    //moving to networkAccessManager thread
    moveToThread( AxisCameraPlugin::instance()->networkAccessManager()->thread() );
}

AxisRelayIOManager::~AxisRelayIOManager()
{
}

void* AxisRelayIOManager::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_CameraRelayIOManager, sizeof(nxcip::IID_CameraRelayIOManager) ) == 0 )
    {
        addRef();
        return static_cast<nxcip::CameraRelayIOManager*>(this);
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

unsigned int AxisRelayIOManager::addRef()
{
    return m_refManager.addRef();
}

unsigned int AxisRelayIOManager::releaseRef()
{
    return m_refManager.releaseRef();
}

int AxisRelayIOManager::getRelayOutputList( char** idList, int* idNum ) const
{
    copyPortList( idList, idNum, m_outputPortNameToIndex );
    return nxcip::NX_NO_ERROR;
}

int AxisRelayIOManager::getInputPortList( char** idList, int* idNum ) const
{
    copyPortList( idList, idNum, m_inputPortNameToIndex );
    return nxcip::NX_NO_ERROR;
}

int AxisRelayIOManager::setRelayOutputState(
    const char* outputID,
    int activate,
    unsigned int autoResetTimeoutMS )
{
    std::map<QString, unsigned int>::const_iterator it = m_outputPortNameToIndex.find( QString::fromUtf8(outputID) );
    if( it == m_outputPortNameToIndex.end() )
        return nxcip::NX_UNKNOWN_PORT_NAME;

    QString cmd = QString::fromLatin1("/axis-cgi/io/port.cgi?action=%1:%2").arg(it->second+1).arg(QLatin1String(activate ? "/" : "\\"));
    if( autoResetTimeoutMS > 0 )
    {
        //adding auto-reset
        cmd += QString::number(autoResetTimeoutMS)+QLatin1String(activate ? "\\" : "/");
    }

    SyncHttpClient httpClient(
        AxisCameraPlugin::instance()->networkAccessManager(),
        m_cameraManager->cameraInfo().url,
        DEFAULT_AXIS_API_PORT,
        m_cameraManager->credentials() );
    if( httpClient.get( QUrl(cmd) ) != QNetworkReply::NoError )
        return nxcip::NX_NETWORK_ERROR;
    if( httpClient.statusCode() != SyncHttpClient::HTTP_OK )
        return httpClient.statusCode() == SyncHttpClient::HTTP_NOT_AUTHORIZED ? nxcip::NX_NOT_AUTHORIZED : nxcip::NX_OTHER_ERROR; 

    return nxcip::NX_NO_ERROR;
}

int AxisRelayIOManager::startInputPortMonitoring()
{
    //we can use QNetworkAccessManaget from owning thread only, so performing async call
        //this method has surely been called not from thread, object belongs to, 
        //so performing asynchronous call and waiting for its completion

    int result = 0;
    callSlotFromOwningThread( "startInputPortMonitoringPriv", &result );
    return result;
}

void AxisRelayIOManager::stopInputPortMonitoring()
{
    //performing async call, see notes in startInputPortMonitoring for details
    callSlotFromOwningThread( "stopInputPortMonitoringPriv" );
}

void AxisRelayIOManager::registerEventHandler( nxcip::CameraInputEventHandler* handler )
{
    QMutexLocker lk( &m_mutex );
    if( std::find(m_eventHandlers.begin(), m_eventHandlers.end(), handler) != m_eventHandlers.end() )
        return;
    m_eventHandlers.push_back( handler );
}

void AxisRelayIOManager::unregisterEventHandler( nxcip::CameraInputEventHandler* handler )
{
    QMutexLocker lk( &m_mutex );
    std::list<nxcip::CameraInputEventHandler*>::iterator it = std::find( m_eventHandlers.begin(), m_eventHandlers.end(), handler );
    if( it != m_eventHandlers.end() )
        m_eventHandlers.erase( it );
}

void AxisRelayIOManager::getLastErrorString( char* errorString ) const
{
    //TODO/IMPL
    errorString[0] = '\0';
}

void AxisRelayIOManager::copyPortList(
    char** idList,
    int* idNum,
    const std::map<QString, unsigned int>& portNameToIndex ) const
{
    *idNum = 0;
    const std::map<QString, unsigned int>::const_iterator itEnd = portNameToIndex.end();
    for( std::map<QString, unsigned int>::const_iterator
        it = portNameToIndex.begin();
        it != itEnd && *idNum < nxcip::MAX_RELAY_PORT_COUNT;
        ++it, ++(*idNum) )
    {
        const QByteArray& nameUtf8 = it->first.toUtf8();
        strncpy( idList[*idNum], nameUtf8.data(), nxcip::MAX_ID_LEN-1 );
        idList[*idNum][nxcip::MAX_ID_LEN-1] = '\0';
    }
}

void AxisRelayIOManager::callSlotFromOwningThread( const char* slotName, int* const resultCode )
{
    QMutexLocker lk( &m_mutex );

    const int asyncCallID = m_asyncCallCounter.fetchAndAddOrdered(1);
    QMetaObject::invokeMethod( this, slotName, Qt::QueuedConnection, Q_ARG(int, asyncCallID) );
    std::map<int, AsyncCallContext>::iterator asyncCallIter = m_awaitedAsyncCallIDs.insert( std::make_pair( asyncCallID, AsyncCallContext() ) ).first;
    //waiting for call to complete
    while( !asyncCallIter->second.done )
        m_cond.wait( lk.mutex() );
    if( resultCode )
        *resultCode = asyncCallIter->second.resultCode;
    m_awaitedAsyncCallIDs.erase( asyncCallIter );
}

void AxisRelayIOManager::startInputPortMonitoringPriv( int asyncCallID )
{
    connect(
        AxisCameraPlugin::instance()->networkAccessManager(), SIGNAL(finished(QNetworkReply*)),
        this, SLOT(onConnectionFinished(QNetworkReply*)) );

    const QAuthenticator& auth = m_cameraManager->credentials();
    QUrl requestUrl;
    requestUrl.setScheme( QLatin1String("http") );
    requestUrl.setHost( QString::fromUtf8(m_cameraManager->cameraInfo().url) );
    requestUrl.setPort( DEFAULT_AXIS_API_PORT );
    for( std::map<QString, unsigned int>::const_iterator
        it = m_inputPortNameToIndex.begin();
        it != m_inputPortNameToIndex.end();
        ++it )
    {
        QMutexLocker lk( &m_mutex );
        std::pair<std::map<unsigned int, QNetworkReply*>::iterator, bool>
            p = m_inputPortHttpMonitor.insert( std::make_pair( it->second, (QNetworkReply*)NULL ) );
        if( !p.second )
            continue;   //port already monitored
        lk.unlock();

        //it is safe to proceed futher with no lock because stopInputMonitoring can be only called from current thread 
            //and forgetHttpClient cannot be called before doGet call

        //requestUrl.setPath( QString::fromLatin1("/axis-cgi/io/port.cgi?monitor=%1").arg(it->second) );
        requestUrl.setPath( QLatin1String("/axis-cgi/io/port.cgi") );

        QUrlQuery requestUrlQuery( requestUrl.query() );
        requestUrlQuery.addQueryItem( QLatin1String("monitor"), QString::number(it->second) );
        requestUrl.setQuery( requestUrlQuery );
        requestUrl.setUserName( auth.user() );
        requestUrl.setPassword( auth.password() );
        QNetworkRequest request;
        request.setUrl( requestUrl );

        p.first->second = AxisCameraPlugin::instance()->networkAccessManager()->get( request );
        connect( p.first->second, SIGNAL(readyRead()), this, SLOT(onMonitorDataAvailable()) );
    }

    //return nxcip::NX_NO_ERROR;

    QMutexLocker lk( &m_mutex );
    std::map<int, AsyncCallContext>::iterator asyncCallIter = m_awaitedAsyncCallIDs.find( asyncCallID );
    NX_ASSERT( asyncCallIter != m_awaitedAsyncCallIDs.end() );
    asyncCallIter->second.resultCode = nxcip::NX_NO_ERROR;
    asyncCallIter->second.done = true;
    m_cond.wakeAll();
}

void AxisRelayIOManager::stopInputPortMonitoringPriv( int asyncCallID )
{
    QMutexLocker lk( &m_mutex );

    QNetworkReply* httpClient = NULL;
    while( !m_inputPortHttpMonitor.empty() )
    {
        httpClient = m_inputPortHttpMonitor.begin()->second;
        m_inputPortHttpMonitor.erase( m_inputPortHttpMonitor.begin() );
        lk.unlock();    //TODO/IMPL is it really needed?
        httpClient->deleteLater();
        lk.relock();
    }

    disconnect( AxisCameraPlugin::instance()->networkAccessManager(), SIGNAL(finished(QNetworkReply*)), this, SLOT(onConnectionFinished(QNetworkReply*)) );

    std::map<int, AsyncCallContext>::iterator asyncCallIter = m_awaitedAsyncCallIDs.find( asyncCallID );
    NX_ASSERT( asyncCallIter != m_awaitedAsyncCallIDs.end() );
    asyncCallIter->second.done = true;
    m_cond.wakeAll();
}

void AxisRelayIOManager::onMonitorDataAvailable()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(QObject::sender());
    NX_ASSERT( reply );

    while( reply->canReadLine() )
    {
        const QByteArray& line = reply->readLine().trimmed();
        switch( m_multipartedParsingState )
        {
            case waitingDelimiter:
                if( line.isEmpty() )
                    continue;
                m_multipartedParsingState = readingHeaders;
                break;

            case readingHeaders:
                if( line.isEmpty() )
                    m_multipartedParsingState = readingData;
                //else ignoring header, since we exactly know format
                break;

            case readingData:
                if( line.isEmpty() )
                    m_multipartedParsingState = waitingDelimiter;
                else
                    readAxisRelayPortNotification( line );
                break;
        }
    }
}

void AxisRelayIOManager::onConnectionFinished( QNetworkReply* reply )
{
    //removing reply
    QMutexLocker lk( &m_mutex );

    for( std::map<unsigned int, QNetworkReply*>::iterator
        it = m_inputPortHttpMonitor.begin();
        it != m_inputPortHttpMonitor.end();
        ++it )
    {
        if( it->second == reply )
        {
            reply->deleteLater();
            m_inputPortHttpMonitor.erase( it );
            return;
        }
    }

    //TODO/IMPL need to reconnect
}

void AxisRelayIOManager::readAxisRelayPortNotification( const QByteArray& notification )
{
    /* notification has format 1I:H, 1I:L, 1I:/, 1I:\  */

    int sepPos = notification.indexOf(':');
    if( sepPos == -1 || sepPos < 2 || sepPos+1 >= notification.size() )
        return; //Error parsing notification: event type not found

    const char eventType = notification[sepPos+1];
    //size_t portTypePos = nx::network::http::find_first_not_of( notification, "0123456789" );
    //if( portTypePos == nx::utils::BufferNpos )
    //    return; //Error parsing notification: port type not found

    //const unsigned int portNumber = notification.mid(0, portTypePos).toUInt();
    //const char portType = notification[portTypePos];

    const unsigned int portNumber = notification.mid(0, 1).toUInt() - 1;    //in notification port numbers start with 1
    const char portType = notification[1];

    if( portType != 'I' )
        return;

    switch( eventType )
    {
        case '/':
        case '\\':
            for( std::list<nxcip::CameraInputEventHandler*>::const_iterator
                it = m_eventHandlers.begin();
                it != m_eventHandlers.end();
                ++it )
            {
                //resolving portNumber to port id
                QByteArray portName;
                for( std::map<QString, unsigned int>::const_iterator
                    jt = m_inputPortNameToIndex.begin();
                    jt != m_inputPortNameToIndex.end();
                    ++jt )
                {
                    if( jt->second == portNumber )
                        portName = jt->first.toUtf8();
                }

                (*it)->inputPortStateChanged(
                    this,
                    portName.constData(),
                    eventType == '/' ? 1 : 0,
                    QDateTime::currentMSecsSinceEpoch() );
            }
            break;

        default:
            break;
    }
}
