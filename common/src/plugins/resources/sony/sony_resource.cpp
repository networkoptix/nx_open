
#include "sony_resource.h"

#include <QtCore/QMutexLocker>

#include "onvif/soapMediaBindingProxy.h"
#include <utils/network/http/asynchttpclient.h>


int QnPlSonyResource::MAX_RESOLUTION_DECREASES_NUM = 3;
static const int INPUT_MONITOR_TIMEOUT_SEC = 5;

using namespace nx_http;

QnPlSonyResource::QnPlSonyResource()
{
}

QnPlSonyResource::~QnPlSonyResource() {

}

CameraDiagnostics::Result QnPlSonyResource::updateResourceCapabilities()
{
    CameraDiagnostics::Result result = QnPlOnvifResource::updateResourceCapabilities();
    if (!result)
        return result;

    std::string confToken = getPrimaryVideoEncoderId().toStdString();
    if (confToken.empty()) {
        return CameraDiagnostics::RequestFailedResult(QLatin1String("getPrimaryVideoEncoderId"), QString());
    }

    QAuthenticator auth(getAuth());
    std::string login = auth.user().toStdString();
    std::string password = auth.password().toStdString();
    std::string endpoint = getMediaUrl().toStdString();

    MediaSoapWrapper soapWrapperGet(endpoint.c_str(), login, password, getTimeDrift());
    VideoConfigReq confRequest;
    confRequest.ConfigurationToken = confToken;
    VideoConfigResp confResponse;

    int soapRes = soapWrapperGet.getVideoEncoderConfiguration(confRequest, confResponse);
    if (soapRes != SOAP_OK || !confResponse.Configuration || !confResponse.Configuration->Resolution) {
        qWarning() << "QnPlSonyResource::updateResourceCapabilities: can't get video encoder (or received data is null) (token="
            << confToken.c_str() << ") from camera (URL: " << soapWrapperGet.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). GSoap error code: " << soapRes << ". " << soapWrapperGet.getLastError();
        return CameraDiagnostics::RequestFailedResult(QLatin1String("getVideoEncoderConfiguration"), soapWrapperGet.getLastError());
    }

    typedef QSharedPointer<QList<QSize> > ResolutionListPtr;
    ResolutionListPtr resolutionListPtr(0);
    {
        QMutexLocker lock(&m_mutex);
        resolutionListPtr = ResolutionListPtr(new QList<QSize>(m_resolutionList)); //Sorted desc
    }

    MediaSoapWrapper soapWrapper(endpoint.c_str(), login, password, getTimeDrift());
    SetVideoConfigReq request;
    request.Configuration = confResponse.Configuration;
    request.Configuration->Encoding = getCodec(true) == H264 ? onvifXsd__VideoEncoding__H264 : onvifXsd__VideoEncoding__JPEG;
    request.ForcePersistence = false;
    SetVideoConfigResp response;

    int triesNumLeft = MAX_RESOLUTION_DECREASES_NUM;
    QList<QSize>::iterator it = resolutionListPtr->begin();

    for (; it != resolutionListPtr->end() && triesNumLeft > 0; --triesNumLeft)
    {
        request.Configuration->Resolution->Width = it->width();
        request.Configuration->Resolution->Height = it->height();

        int retryCount = getMaxOnvifRequestTries();
        soapRes = SOAP_ERR;

        while (soapRes != SOAP_OK && --retryCount >= 0)
        {
            soapRes = soapWrapper.setVideoEncoderConfiguration(request, response);
            if (soapRes != SOAP_OK) {
                if (soapWrapper.isConflictError()) {
                    continue;
                }

                qWarning() << "QnPlSonyResource::updateResourceCapabilities: can't set video encoder options (token="
                    << confToken.c_str() << ") from camera (URL: " << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
                    << "). GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();
                return CameraDiagnostics::RequestFailedResult(
                    QString::fromLatin1("setVideoEncoderConfiguration(%1x%2)").arg(it->width()).arg(it->height()),
                    soapWrapper.getLastError() );
            }
        }

        if (soapRes == SOAP_OK) {
            break;
        }

        qWarning() << "QnPlSonyResource::updateResourceCapabilities: resolution " << it->width()
            << " x " << it->height() << " dropped. UniqueId: " << getUniqueId();
        it = resolutionListPtr->erase(it);
    }

    if (soapRes != SOAP_OK) {
        return CameraDiagnostics::RequestFailedResult( QString::fromLatin1("setVideoEncoderConfiguration"), soapWrapper.getLastError() );
    }

    if (triesNumLeft == MAX_RESOLUTION_DECREASES_NUM) {
        return CameraDiagnostics::NoErrorResult();
    }

    QMutexLocker lock(&m_mutex);
    m_resolutionList = *resolutionListPtr.data();

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnPlSonyResource::initInternal()
{
    const CameraDiagnostics::Result result = QnPlOnvifResource::initInternal();
    if( result.errorCode != CameraDiagnostics::ErrorCode::noError )
        return result;

    //TODO/IMPL if no input, exiting

    CLSimpleHTTPClient http(
        getHostAddress(),
        QUrl(getUrl()).port(DEFAULT_HTTP_PORT),
        getNetworkTimeout(),
        getAuth() );
    //turning on input monitoring
    CLHttpStatus status = http.doGET( QLatin1String("/command/system.cgi?AlarmData=on") );
    if( status % 100 != 2 )
    {
        NX_LOG( QString::fromLatin1("Failed to execute /command/system.cgi?AlarmData=on on Sony camera %1. http status %2").
            arg(getHostAddress()).arg(status), cl_logDEBUG1 );
    }

    startInputPortMonitoring();

    return CameraDiagnostics::NoErrorResult();
}

bool QnPlSonyResource::startInputPortMonitoring()
{
    QMutexLocker lk( &m_inputPortMutex );

    if( isDisabled()
        || hasFlags(QnResource::foreigner) )     //we do not own camera
    {
        return false;
    }

    if( m_inputMonitorHttpClient )
    {
        m_inputMonitorHttpClient->terminate();
        m_inputMonitorHttpClient.reset();
    }

    const QAuthenticator& auth = getAuth();
    QUrl requestUrl;
    requestUrl.setHost( getHostAddress() );
    requestUrl.setPort( QUrl(getUrl()).port(DEFAULT_HTTP_PORT) );

        //it is safe to proceed with no lock futher because stopInputMonitoring can be only called from current thread 
            //and forgetHttpClient cannot be called before doGet call

    requestUrl.setPath( QString::fromLatin1("/command/alarmdata.cgi?interval=%1").arg(INPUT_MONITOR_TIMEOUT_SEC) );
    m_inputMonitorHttpClient = std::make_shared<nx_http::AsyncHttpClient>();
    connect( m_inputMonitorHttpClient.get(), SIGNAL(responseReceived(nx_http::AsyncHttpClient*)),          this, SLOT(onMonitorResponseReceived(nx_http::AsyncHttpClient*)),        Qt::DirectConnection );
    connect( m_inputMonitorHttpClient.get(), SIGNAL(someMessageBodyAvailable(nx_http::AsyncHttpClient*)),  this, SLOT(onMonitorMessageBodyAvailable(nx_http::AsyncHttpClient*)),    Qt::DirectConnection );
    connect( m_inputMonitorHttpClient.get(), SIGNAL(done(nx_http::AsyncHttpClient*)),                      this, SLOT(onMonitorConnectionClosed(nx_http::AsyncHttpClient*)),        Qt::DirectConnection );
    m_inputMonitorHttpClient->setTotalReconnectTries( AsyncHttpClient::UNLIMITED_RECONNECT_TRIES );
    m_inputMonitorHttpClient->setUserName( auth.user() );
    m_inputMonitorHttpClient->setUserPassword( auth.password() );
    return m_inputMonitorHttpClient->doGet( requestUrl );
}

void QnPlSonyResource::stopInputPortMonitoring()
{
    std::shared_ptr<nx_http::AsyncHttpClient> inputMonitorHttpClient = m_inputMonitorHttpClient;
    {
        QMutexLocker lk( &m_inputPortMutex );
        if( !m_inputMonitorHttpClient )
            return;
        m_inputMonitorHttpClient.reset();
    }
    //calling terminate with m_inputPortMutex locked can lead to dead-lock with onMonitorResponseReceived method, called from http event thread
    inputMonitorHttpClient->terminate();
    inputMonitorHttpClient.reset();
}

bool QnPlSonyResource::isInputPortMonitored() const
{
    QMutexLocker lk( &m_inputPortMutex );
    return m_inputMonitorHttpClient != NULL;
}

void QnPlSonyResource::onMonitorResponseReceived( AsyncHttpClient* httpClient )
{
    QMutexLocker lk( &m_inputPortMutex );

    if( m_inputMonitorHttpClient.get() != httpClient )    //this can happen just after stopInputPortMonitoring() call
        return;

    if( (m_inputMonitorHttpClient->response()->statusLine.statusCode / 100) * 100 != StatusCode::ok )
    {
        cl_log.log( QString::fromLatin1("Sony camera %1. Failed to subscribe to input monitoring. %3").
            arg(getUrl()).arg(QLatin1String(m_inputMonitorHttpClient->response()->statusLine.reasonPhrase)), cl_logDEBUG1 );
        m_inputMonitorHttpClient.reset();
        return;
    }

    //TODO/IMPL multipart/x-mixed-replace support

    m_lineSplitter.reset();
    m_inputMonitorHttpClient->startReadMessageBody();
}

void QnPlSonyResource::onMonitorMessageBodyAvailable( AsyncHttpClient* httpClient )
{
    QMutexLocker lk( &m_inputPortMutex );

    if( m_inputMonitorHttpClient.get() != httpClient )
        return;

    const BufferType& msgBodyBuf = httpClient->fetchMessageBodyBuffer();
    for( int offset = 0; offset < msgBodyBuf.size(); )
    {
        size_t bytesRead = 0;
        QnByteArrayConstRef lineBuffer;
        const bool isLineRead = m_lineSplitter.parseByLines(
            ConstBufferRefType(msgBodyBuf, offset),
            &lineBuffer,
            &bytesRead );
        offset += (int)bytesRead;
        if( !isLineRead )
        {
            //no full line yet
            continue;
        }

        //lineBuffer holds the line
        const BufferType& line = lineBuffer.toByteArrayWithRawData();
        int sepPos = line.indexOf('=');
        if( sepPos == -1 )
            continue;   //unknown format
        const BufferType& paramName = BufferType::fromRawData( line.data(), sepPos );
        const BufferType& paramValue = BufferType::fromRawData( line.data()+sepPos+1, line.size()-(sepPos+1) );

        if( !paramName.startsWith("Sensor") )
            continue;
        const int inputPortIndex = paramName.mid(6).toInt();
        const bool currentPortState = paramValue != "0";
        bool& prevPortState = m_relayInputStates[inputPortIndex];
        if( currentPortState == prevPortState )
            continue;
        prevPortState = currentPortState;

        emit cameraInput(
            toSharedPointer(),
            QString::number(inputPortIndex),
            currentPortState,
            QDateTime::currentMSecsSinceEpoch() );
    }
}

void QnPlSonyResource::onMonitorConnectionClosed( AsyncHttpClient* /*httpClient*/ )
{
    //TODO/IMPL reconnect
}
