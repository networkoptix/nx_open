/**********************************************************
* 12 apr 2013
* akolesnikov
***********************************************************/

#include "relayio_manager.h"

#include <algorithm>
#include <cstring>

#include <QtCore/QDateTime>
#include <QtCore/QUrlQuery>

#include "camera_manager.h"
#include "cam_params.h"
#include "camera_plugin.h"
#include "sync_http_client.h"


static const int RELAY_INPUT_CHECK_TIMEOUT_MS = 500;

RelayIOManager::RelayIOManager(
    CameraManager* cameraManager,
    const std::vector<QByteArray>& relayParamsStr )
:
    m_refManager( cameraManager->refManager() ),
    m_cameraManager( cameraManager ),
    m_inputPortCount( 0 ),
    m_outputPortCount( 0 ),
    m_multipartedParsingState( waitingDelimiter )
{
    for( const QByteArray& paramVal: relayParamsStr )
    {
        const QList<QByteArray>& paramValueList = paramVal.split( '=' );
        if( paramValueList.size() < 2 )
            continue;

        if( paramValueList[0] == "root.Input.NbrOfInputs" )
            m_inputPortCount = paramValueList[1].toInt();
        else if( paramValueList[0].startsWith("root.Input.I") )
        {
            const int portNumber = paramValueList[0].mid( sizeof("root.Input.I")-1 ).toInt();
            const QByteArray& inputFieldName = paramValueList[0].mid(paramValueList[0].lastIndexOf('.')+1);
            if( inputFieldName == "Name" )
                m_inputPortNameToIndex[QLatin1String(paramValueList[1])] = portNumber+1;
            //else if( inputFieldName == "Trig" )
            //    ;
        }
        else if( paramValueList[0] == "root.Output.NbrOfOutputs" )
            m_outputPortCount = paramValueList[1].toInt();
        else if( paramValueList[0].startsWith("root.Output.O") )
        {
            const int portNumber = paramValueList[0].mid( sizeof("root.Output.O")-1 ).toInt();
            const QByteArray& outputFieldName = paramValueList[0].mid(paramValueList[0].lastIndexOf('.')+1);
            if( outputFieldName == "Name" )
                m_outputPortNameToIndex[QLatin1String(paramValueList[1])] = portNumber+1;
            //else if( outputFieldName == "Trig" )
            //    ;
        }
    }


#if 0
    std::unique_ptr<SyncHttpClient> httpClient;
    if( !httpClient.get() )
        httpClient.reset( new SyncHttpClient(
            CameraPlugin::instance()->networkAccessManager(),
            m_cameraManager->cameraInfo().url,
            DEFAULT_CAMERA_API_PORT,
            m_cameraManager->credentials() ) );

    //reading port direction and names
    for( unsigned int i = 0; i < m_inputPortCount+m_outputPortCount; ++i )
    {
        QByteArray portDirection;
        int status = CameraManager::fetchCameraParameter(
            httpClient.get(),
            QString::fromLatin1("IOPort.I%1.Direction").arg(i).toLatin1(),
            &portDirection );
        if( status != SyncHttpClient::HTTP_OK )
            continue;

        QString portName;
        status = CameraManager::fetchCameraParameter(
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
#endif

    //moving to networkAccessManager thread
    moveToThread( CameraPlugin::instance()->qtEventLoopThread()  /*CameraPlugin::instance()->networkAccessManager()->thread()*/ );

    m_inputCheckTimer.moveToThread( CameraPlugin::instance()->qtEventLoopThread() );
    m_inputCheckTimer.setInterval( RELAY_INPUT_CHECK_TIMEOUT_MS );
    m_inputCheckTimer.setSingleShot( false );
    connect( &m_inputCheckTimer, &QTimer::timeout, this, &RelayIOManager::onTimer );
}

RelayIOManager::~RelayIOManager()
{
}

void* RelayIOManager::queryInterface( const nxpl::NX_GUID& interfaceID )
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

unsigned int RelayIOManager::addRef()
{
    return m_refManager.addRef();
}

unsigned int RelayIOManager::releaseRef()
{
    return m_refManager.releaseRef();
}

int RelayIOManager::getRelayOutputList( char** idList, int* idNum ) const
{
    copyPortList( idList, idNum, m_outputPortNameToIndex );
    return nxcip::NX_NO_ERROR;
}

int RelayIOManager::getInputPortList( char** idList, int* idNum ) const
{
    copyPortList( idList, idNum, m_inputPortNameToIndex );
    return nxcip::NX_NO_ERROR;
}

int RelayIOManager::setRelayOutputState(
    const char* outputID,
    int activate,
    unsigned int autoResetTimeoutMS )
{
    std::map<QString, unsigned int>::const_iterator it = m_outputPortNameToIndex.find( QString::fromUtf8(outputID) );
    if( it == m_outputPortNameToIndex.end() )
        return nxcip::NX_UNKNOWN_PORT_NAME;

    {
        QMutexLocker lk( &m_mutex );
        //TODO/IMPL cancelling reset timer
    }

    QString cmd = QString::fromLatin1("cgi-bin/io/output.cgi?action=%1:%2").arg(it->second+1).arg(QLatin1String(activate ? "/" : "\\"));

    SyncHttpClient httpClient(
        CameraPlugin::instance()->networkAccessManager(),
        m_cameraManager->cameraInfo().url,
        DEFAULT_CAMERA_API_PORT,
        m_cameraManager->credentials() );
    if( httpClient.get( QUrl(cmd) ) != QNetworkReply::NoError )
        return nxcip::NX_NETWORK_ERROR;
    if( httpClient.statusCode() != SyncHttpClient::HTTP_OK )
        return httpClient.statusCode() == SyncHttpClient::HTTP_NOT_AUTHORIZED ? nxcip::NX_NOT_AUTHORIZED : nxcip::NX_OTHER_ERROR; 

    if( autoResetTimeoutMS > 0 )
    {
        //TODO/IMPL launching auto-reset timer
        QMutexLocker lk( &m_mutex );
    }

    return nxcip::NX_NO_ERROR;
}

int RelayIOManager::startInputPortMonitoring()
{
    //we can use QNetworkAccessManager from owning thread only, so performing async call
        //this method has surely been called not from thread, object belongs to, 
        //so performing asynchronous call and waiting for its completion

    //int result = 0;
    //callSlotFromOwningThread( "startInputPortMonitoringPriv", &result );
    //return result;

    //TODO/IMPL checking current port state

    QMetaObject::invokeMethod( &m_inputCheckTimer, "start", Qt::QueuedConnection );
    return nxcip::NX_NO_ERROR;
}

void RelayIOManager::stopInputPortMonitoring()
{
    QMetaObject::invokeMethod( &m_inputCheckTimer, "stop", Qt::QueuedConnection );
}

void RelayIOManager::registerEventHandler( nxcip::CameraInputEventHandler* handler )
{
    QMutexLocker lk( &m_mutex );
    if( std::find(m_eventHandlers.begin(), m_eventHandlers.end(), handler) != m_eventHandlers.end() )
        return;
    m_eventHandlers.push_back( handler );
}

void RelayIOManager::unregisterEventHandler( nxcip::CameraInputEventHandler* handler )
{
    QMutexLocker lk( &m_mutex );
    std::list<nxcip::CameraInputEventHandler*>::iterator it = std::find( m_eventHandlers.begin(), m_eventHandlers.end(), handler );
    if( it != m_eventHandlers.end() )
        m_eventHandlers.erase( it );
}

void RelayIOManager::getLastErrorString( char* errorString ) const
{
    //TODO/IMPL
    errorString[0] = '\0';
}

void RelayIOManager::copyPortList(
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

void RelayIOManager::callSlotFromOwningThread( const char* slotName, int* const resultCode )
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

void RelayIOManager::onTimer()
{
    SyncHttpClient httpClient(
        CameraPlugin::instance()->networkAccessManager(),
        m_cameraManager->cameraInfo().url,
        DEFAULT_CAMERA_API_PORT,
        m_cameraManager->credentials() );
    for( unsigned portNumber = 1; portNumber <= m_inputPortCount; ++portNumber )
    {
        if( httpClient.get( QString::fromLatin1("/cgi-bin/io/input.cgi?check=%1").arg(portNumber) ) != QNetworkReply::NoError )
            return;
        if( httpClient.statusCode() != SyncHttpClient::HTTP_OK )
            return; 
        const QByteArray& body = httpClient.readWholeMessageBody().trimmed();
        const QList<QByteArray>& paramItems = body.split('=');
        if( paramItems.size() < 2 )
            continue;
        const int newPortState = paramItems[1].toInt();
        int& currentPortState = m_inputPortState[portNumber];
        if( currentPortState != newPortState )
        {
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
                    newPortState,
                    QDateTime::currentMSecsSinceEpoch() );
            }
            currentPortState = newPortState;
        }
    }
}
