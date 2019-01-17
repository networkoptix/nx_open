#ifdef ENABLE_ONVIF

#include "sony_resource.h"

#include <nx/utils/thread/mutex.h>

#include <nx/utils/log/log.h>

#include "onvif/soapMediaBindingProxy.h"
#include "utils/common/synctime.h"

int QnPlSonyResource::MAX_RESOLUTION_DECREASES_NUM = 3;
static const int INPUT_MONITOR_TIMEOUT_SEC = 5;

using namespace nx::network::http;

QnPlSonyResource::QnPlSonyResource(QnMediaServerModule* serverModule):
    QnPlOnvifResource(serverModule)
{
    setVendor(lit("Sony"));
}

QnPlSonyResource::~QnPlSonyResource() {
    nx::network::http::AsyncHttpClientPtr inputMonitorHttpClient;
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

    auto capabilities = primaryVideoCapabilities();
    std::string confToken = capabilities.videoEncoderToken.toStdString();
    if (confToken.empty())
        return CameraDiagnostics::RequestFailedResult(QLatin1String("getPrimaryVideoEncoderId"), QString());

    MediaSoapWrapper soapWrapperGet(this);
    VideoConfigReq confRequest;
    confRequest.ConfigurationToken = confToken;
    VideoConfigResp confResponse;

    int soapRes = soapWrapperGet.getVideoEncoderConfiguration(confRequest, confResponse);
    if (soapRes != SOAP_OK || !confResponse.Configuration || !confResponse.Configuration->Resolution) {
        qWarning() << "QnPlSonyResource::updateResourceCapabilities: can't get video encoder (or received data is null) (token="
            << confToken.c_str() << ") from camera (URL: " << soapWrapperGet.endpoint() << ", UniqueId: " << getUniqueId()
            << "). GSoap error code: " << soapRes << ". " << soapWrapperGet.getLastErrorDescription();
        return CameraDiagnostics::RequestFailedResult(QLatin1String("getVideoEncoderConfiguration"), soapWrapperGet.getLastErrorDescription());
    }

    MediaSoapWrapper soapWrapper(this);
    SetVideoConfigReq request;
    request.Configuration = confResponse.Configuration;
    switch (capabilities.encoding)
    {
        case SupportedVideoCodecFlavor::JPEG:
            request.Configuration->Encoding = onvifXsd__VideoEncoding::JPEG;
            break;
        case SupportedVideoCodecFlavor::H264:
            request.Configuration->Encoding = onvifXsd__VideoEncoding::H264;
            break;
        default:
            // ONVIF Media1 interface doesn't support other codecs
            // (except MPEG4, but VMS doesn't support it), so we use JPEG in this case,
            // because it is usually supported by all devices
            request.Configuration->Encoding = onvifXsd__VideoEncoding::JPEG;
    }

    request.ForcePersistence = false;
    SetVideoConfigResp response;

    int triesNumLeft = MAX_RESOLUTION_DECREASES_NUM;
    auto it = capabilities.resolutions.begin();

    for (; it != capabilities.resolutions.end() && triesNumLeft > 0; --triesNumLeft)
    {
        request.Configuration->Resolution->Width = it->width();
        request.Configuration->Resolution->Height = it->height();

        int retryCount = getMaxOnvifRequestTries();
        soapRes = SOAP_ERR;

        while (soapRes != SOAP_OK && --retryCount >= 0)
        {
            soapRes = soapWrapper.setVideoEncoderConfiguration(request, response);
            if (soapRes != SOAP_OK)
            {
                if (soapWrapper.lastErrorIsConflict())
                {
                    continue;
                }

                qWarning() << "QnPlSonyResource::updateResourceCapabilities: can't set video encoder options (token="
                    << confToken.c_str() << ") from camera (URL: " << soapWrapper.endpoint() << ", UniqueId: " << getUniqueId()
                    << "). GSoap error code: " << soapRes << ". " << soapWrapper.getLastErrorDescription();
                return CameraDiagnostics::RequestFailedResult(
                    lit("setVideoEncoderConfiguration(%1x%2)").arg(it->width()).arg(it->height()),
                    soapWrapper.getLastErrorDescription() );
            }
        }

        if (soapRes == SOAP_OK)
            break;

        qWarning() << "QnPlSonyResource::updateResourceCapabilities: resolution " << it->width()
            << " x " << it->height() << " dropped. UniqueId: " << getUniqueId();
        it = capabilities.resolutions.erase(it);
    }

    if (soapRes != SOAP_OK)
        return CameraDiagnostics::RequestFailedResult( lit("setVideoEncoderConfiguration"), soapWrapper.getLastErrorDescription() );

    if (triesNumLeft == MAX_RESOLUTION_DECREASES_NUM) {
        return CameraDiagnostics::NoErrorResult();
    }

    setPrimaryVideoCapabilities(capabilities);
    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnPlSonyResource::customInitialization(
    const _onvifDevice__GetCapabilitiesResponse& /*capabilitiesResponse*/)
{
    CameraDiagnostics::Result result = CameraDiagnostics::NoErrorResult();

    //if no input, exiting
    if( !hasCameraCapabilities(Qn::InputPortCapability) )
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
        NX_DEBUG(this, lit("Failed to execute /command/system.cgi?AlarmData=on on Sony camera %1. http status %2").
            arg(getHostAddress()).arg(status));
    }

    return CameraDiagnostics::NoErrorResult();
}

void QnPlSonyResource::startInputPortStatesMonitoring()
{
    QnMutexLocker lk( &m_inputPortMutex );
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
    m_inputMonitorHttpClient = nx::network::http::AsyncHttpClient::create();
    connect(
        m_inputMonitorHttpClient.get(), &nx::network::http::AsyncHttpClient::responseReceived,
        this, &QnPlSonyResource::onMonitorResponseReceived,
        Qt::DirectConnection );
    connect(
        m_inputMonitorHttpClient.get(), &nx::network::http::AsyncHttpClient::someMessageBodyAvailable,
        this, &QnPlSonyResource::onMonitorMessageBodyAvailable,
        Qt::DirectConnection );
    connect(
        m_inputMonitorHttpClient.get(), &nx::network::http::AsyncHttpClient::done,
        this, &QnPlSonyResource::onMonitorConnectionClosed,
        Qt::DirectConnection );
    m_inputMonitorHttpClient->setTotalReconnectTries( AsyncHttpClient::UNLIMITED_RECONNECT_TRIES );
    m_inputMonitorHttpClient->setUserName( auth.user() );
    m_inputMonitorHttpClient->setUserPassword( auth.password() );
    m_inputMonitorHttpClient->doGet( requestUrl );
}

void QnPlSonyResource::stopInputPortStatesMonitoring()
{
    nx::network::http::AsyncHttpClientPtr inputMonitorHttpClient;
    {
        QnMutexLocker lk( &m_inputPortMutex );
        inputMonitorHttpClient = std::move(m_inputMonitorHttpClient);
    }
    //calling terminate with m_inputPortMutex locked can lead to dead-lock with onMonitorResponseReceived method, called from http event thread
    if (inputMonitorHttpClient)
        inputMonitorHttpClient->pleaseStopSync();
}

void QnPlSonyResource::onMonitorResponseReceived( AsyncHttpClientPtr httpClient )
{
    QnMutexLocker lk( &m_inputPortMutex );

    if( m_inputMonitorHttpClient != httpClient )    //this can happen just after stopInputPortStatesMonitoring() call
        return;

    if( (m_inputMonitorHttpClient->response()->statusLine.statusCode / 100) * 100 != StatusCode::ok )
    {
        NX_DEBUG(this, lit("Sony camera %1. Failed to subscribe to input monitoring. %3").
            arg(getUrl()).arg(QLatin1String(m_inputMonitorHttpClient->response()->statusLine.reasonPhrase)));
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

        emit inputPortStateChanged(
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
        response->statusLine.statusCode != nx::network::http::StatusCode::ok)
    {
        return;
    }

    //restoring connection
    m_inputMonitorHttpClient->doGet(m_inputMonitorHttpClient->url());
}

#endif //ENABLE_ONVIF
