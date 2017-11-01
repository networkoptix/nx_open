#ifdef ENABLE_ONVIF

#include "sony_resource.h"

#include <nx/utils/thread/mutex.h>

#include <nx/utils/log/log.h>

#include "onvif/soapMediaBindingProxy.h"
#include "utils/common/synctime.h"


int QnPlSonyResource::MAX_RESOLUTION_DECREASES_NUM = 3;
static const int INPUT_MONITOR_TIMEOUT_SEC = 5;

using namespace nx_http;

QnPlSonyResource::QnPlSonyResource()
{
    setVendor(lit("Sony"));
}

QnPlSonyResource::~QnPlSonyResource() {
    nx_http::AsyncHttpClientPtr inputMonitorHttpClient;
    {
        QnMutexLocker lk(&m_inputPortMutex);
        inputMonitorHttpClient = std::move(m_inputMonitorHttpClient);
    }
    if (inputMonitorHttpClient)
        inputMonitorHttpClient->pleaseStopSync();
}

CameraDiagnostics::Result QnPlSonyResource::updateResourceCapabilities()
{
    CameraDiagnostics::Result result = QnPlOnvifResource::updateResourceCapabilities();
    if (!result || isCameraControlDisabled())
        return result;

    std::string confToken = getPrimaryVideoEncoderId().toStdString();
    if (confToken.empty()) {
        return CameraDiagnostics::RequestFailedResult(QLatin1String("getPrimaryVideoEncoderId"), QString());
    }

    QAuthenticator auth = getAuth();
    QString login = auth.user();
    QString password = auth.password();
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
        QnMutexLocker lock( &m_mutex );
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
                    lit("setVideoEncoderConfiguration(%1x%2)").arg(it->width()).arg(it->height()),
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
        return CameraDiagnostics::RequestFailedResult( lit("setVideoEncoderConfiguration"), soapWrapper.getLastError() );
    }

    if (triesNumLeft == MAX_RESOLUTION_DECREASES_NUM) {
        return CameraDiagnostics::NoErrorResult();
    }

    QnMutexLocker lock( &m_mutex );
    m_resolutionList = *resolutionListPtr.data();

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnPlSonyResource::customInitialization(
    const CapabilitiesResp& /*capabilitiesResponse*/)
{
    CameraDiagnostics::Result result = CameraDiagnostics::NoErrorResult();

    //if no input, exiting
    if( !hasCameraCapabilities(Qn::RelayInputCapability) )
        return result;

    QAuthenticator auth = getAuth();

    CLSimpleHTTPClient http(
        getHostAddress(),
        QUrl(getUrl()).port(DEFAULT_HTTP_PORT),
        getNetworkTimeout(),
        auth );
    //turning on input monitoring
    CLHttpStatus status = http.doGET( QLatin1String("/command/system.cgi?AlarmData=on") );
    if( status % 100 != 2 )
    {
        NX_LOG( lit("Failed to execute /command/system.cgi?AlarmData=on on Sony camera %1. http status %2").
            arg(getHostAddress()).arg(status), cl_logDEBUG1 );
    }

    return CameraDiagnostics::NoErrorResult();
}

bool QnPlSonyResource::startInputPortMonitoringAsync( std::function<void(bool)>&& /*completionHandler*/ )
{
    QnMutexLocker lk( &m_inputPortMutex );

    if( hasFlags(Qn::foreigner) ||      //we do not own camera
        !hasCameraCapabilities(Qn::RelayInputCapability) )
    {
        return false;
    }

    if( m_inputMonitorHttpClient )
    {
        m_inputMonitorHttpClient->pleaseStopSync();
        m_inputMonitorHttpClient.reset();
    }

    QAuthenticator auth = getAuth();
    nx::utils::Url requestUrl;
    requestUrl.setHost( getHostAddress() );
    requestUrl.setPort( QUrl(getUrl()).port(DEFAULT_HTTP_PORT) );

        //it is safe to proceed with no lock futher because stopInputMonitoring can be only called from current thread
            //and forgetHttpClient cannot be called before doGet call

    requestUrl.setPath(lit("/command/alarmdata.cgi"));
    requestUrl.setQuery(lit("interval=%1").arg(INPUT_MONITOR_TIMEOUT_SEC));
    m_inputMonitorHttpClient = nx_http::AsyncHttpClient::create();
    connect(
        m_inputMonitorHttpClient.get(), &nx_http::AsyncHttpClient::responseReceived,
        this, &QnPlSonyResource::onMonitorResponseReceived,
        Qt::DirectConnection );
    connect(
        m_inputMonitorHttpClient.get(), &nx_http::AsyncHttpClient::someMessageBodyAvailable,
        this, &QnPlSonyResource::onMonitorMessageBodyAvailable,
        Qt::DirectConnection );
    connect(
        m_inputMonitorHttpClient.get(), &nx_http::AsyncHttpClient::done,
        this, &QnPlSonyResource::onMonitorConnectionClosed,
        Qt::DirectConnection );
    m_inputMonitorHttpClient->setTotalReconnectTries( AsyncHttpClient::UNLIMITED_RECONNECT_TRIES );
    m_inputMonitorHttpClient->setUserName( auth.user() );
    m_inputMonitorHttpClient->setUserPassword( auth.password() );
    m_inputMonitorHttpClient->doGet( requestUrl );
    return true;
}

void QnPlSonyResource::stopInputPortMonitoringAsync()
{
    nx_http::AsyncHttpClientPtr inputMonitorHttpClient;
    {
        QnMutexLocker lk( &m_inputPortMutex );
        inputMonitorHttpClient = std::move(m_inputMonitorHttpClient);
    }
    //calling terminate with m_inputPortMutex locked can lead to dead-lock with onMonitorResponseReceived method, called from http event thread
    if (inputMonitorHttpClient)
        inputMonitorHttpClient->pleaseStopSync();
}

bool QnPlSonyResource::isInputPortMonitored() const
{
    QnMutexLocker lk( &m_inputPortMutex );
    return m_inputMonitorHttpClient.get() != NULL;
}

void QnPlSonyResource::onMonitorResponseReceived( AsyncHttpClientPtr httpClient )
{
    QnMutexLocker lk( &m_inputPortMutex );

    if( m_inputMonitorHttpClient != httpClient )    //this can happen just after stopInputPortMonitoringAsync() call
        return;

    if( (m_inputMonitorHttpClient->response()->statusLine.statusCode / 100) * 100 != StatusCode::ok )
    {
        NX_LOG( lit("Sony camera %1. Failed to subscribe to input monitoring. %3").
            arg(getUrl()).arg(QLatin1String(m_inputMonitorHttpClient->response()->statusLine.reasonPhrase)), cl_logDEBUG1 );
        m_inputMonitorHttpClient.reset();
        return;
    }

    //TODO/IMPL multipart/x-mixed-replace support

    m_lineSplitter.reset();
}

void QnPlSonyResource::onMonitorMessageBodyAvailable( AsyncHttpClientPtr httpClient )
{
    QnMutexLocker lk( &m_inputPortMutex );

    if( m_inputMonitorHttpClient != httpClient )
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
            qnSyncTime->currentUSecsSinceEpoch() );
    }
}

void QnPlSonyResource::onMonitorConnectionClosed( AsyncHttpClientPtr httpClient )
{
    QnMutexLocker lk(&m_inputPortMutex);

    if (m_inputMonitorHttpClient != httpClient)
        return;

    if (getStatus() < Qn::Online)
        return;

    auto response = httpClient->response();
    if (static_cast<bool>(response) &&
        response->statusLine.statusCode != nx_http::StatusCode::ok)
    {
        return;
    }

    //restoring connection
    m_inputMonitorHttpClient->doGet(m_inputMonitorHttpClient->url());
}

#endif //ENABLE_ONVIF
