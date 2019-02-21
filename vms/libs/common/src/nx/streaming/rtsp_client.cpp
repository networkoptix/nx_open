#if defined(__arm__)
    #include <sys/ioctl.h>
#endif

#include <nx/streaming/rtsp_client.h>
#include <nx/utils/datetime.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/rtsp/rtsp_types.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/utils/uuid.h>

#include <network/tcp_connection_priv.h>
#include <network/tcp_connection_processor.h>
#include <utils/common/sleep.h>
#include <nx/network/nettools.h>

#define DEFAULT_RTP_PORT 554
#define RESERVED_TIMEOUT_TIME (10*1000)

static const int TCP_RECEIVE_TIMEOUT_MS = 1000 * 5;
static const int TCP_CONNECT_TIMEOUT_MS = 1000 * 5;
static const int SDP_TRACK_STEP = 2;

QByteArray QnRtspClient::m_guid;
QnMutex QnRtspClient::m_guidMutex;

namespace {
const QString METADATA_STR(lit("ffmpeg-metadata"));

struct RtspPorts
{
    quint16 mediaPort = 0;
    quint16 rtcpPort = 0;
};

RtspPorts parsePorts(const QString& parameterString)
{
    const auto nameAndValue = parameterString.splitRef('=');
    if (nameAndValue.size() != 2)
        return {};

    const auto ports = nameAndValue[1].split('-');
    if (nameAndValue.size() != 2)
        return {};

    const auto mediaPort = ports[0].toUInt();
    const auto rtcpPort = ports[1].toUInt();

    if (!mediaPort || !rtcpPort)
        return {};

    return {mediaPort, rtcpPort};
}

} // namespace

const QByteArray QnRtspClient::kPlayCommand("PLAY");
const QByteArray QnRtspClient::kSetupCommand("SETUP");
const QByteArray QnRtspClient::kOptionsCommand("OPTIONS");
const QByteArray QnRtspClient::kDescribeCommand("DESCRIBE");
const QByteArray QnRtspClient::kSetParameterCommand("SET_PARAMETER");
const QByteArray QnRtspClient::kGetParameterCommand("GET_PARAMETER");
const QByteArray QnRtspClient::kPauseCommand("PAUSE");
const QByteArray QnRtspClient::kTeardownCommand("TEARDOWN");

RtspTransport rtspTransportFromString(const QString& value)
{
    auto upperValue = value.toUpper().trimmed();
    if (upperValue == "TCP")
        return RtspTransport::tcp;
    if (upperValue == "UDP")
        return RtspTransport::udp;
    if (upperValue == "MULTICAST")
        return RtspTransport::multicast;

    NX_ASSERT(upperValue.isEmpty(), lm("Unsupported value: %1").arg(value));
    return RtspTransport::notDefined;
}

QString toString(const RtspTransport& value)
{
    switch (value)
    {
        case RtspTransport::notDefined:
            return QString();
        case RtspTransport::udp:
            return "UDP";
        case RtspTransport::tcp:
            return "TCP";
        case RtspTransport::multicast:
            return "MULTICAST";
    }

    const auto s = lm("TRANSPORT_%1").arg(static_cast<int>(value));
    NX_ASSERT(false, lm("Unsupported value: %1").arg(s));
    return s;
}

//-------------------------------------------------------------------------------------------------
// QnRtspIoDevice

QnRtspIoDevice::QnRtspIoDevice(QnRtspClient* owner, RtspTransport transport, quint16 mediaPort, quint16 rtcpPort):
    m_owner(owner),
    m_remoteMediaPort(mediaPort),
    m_remoteRtcpPort(rtcpPort)
{
    if (m_remoteRtcpPort == 0 && m_remoteMediaPort != 0)
        m_remoteRtcpPort = m_remoteMediaPort + 1;
    setTransport(transport);
}

QnRtspIoDevice::~QnRtspIoDevice()
{
    if (m_transport == RtspTransport::multicast)
    {
        if (m_mediaSocket)
            m_mediaSocket->leaveGroup(m_multicastAddress.toString());
        if (m_rtcpSocket)
            m_rtcpSocket->leaveGroup(m_multicastAddress.toString());
    }
}

void QnRtspIoDevice::bindToMulticastAddress(const QHostAddress& address, const QString& interfaceAddress)
{
    if (m_mediaSocket)
        m_mediaSocket->joinGroup(address.toString(), interfaceAddress);
    if (m_rtcpSocket)
        m_rtcpSocket->joinGroup(address.toString(), interfaceAddress);

    m_multicastAddress = address;
}

qint64 QnRtspIoDevice::read(char *data, qint64 maxSize)
{
    int bytesRead;
    if (m_transport == RtspTransport::tcp)
    {
        bytesRead = m_owner->readBinaryResponce((quint8*) data, maxSize); // demux binary data from TCP socket
    }
    else
    {
        bytesRead = m_mediaSocket->recv(data, maxSize);
    }
    m_owner->sendKeepAliveIfNeeded();
    if (m_transport == RtspTransport::udp)
        processRtcpData();
    return bytesRead;
}

nx::network::AbstractCommunicatingSocket* QnRtspIoDevice::getMediaSocket()
{
    if (m_transport == RtspTransport::tcp)
        return m_owner->m_tcpSock.get();
    else
        return m_mediaSocket.get();
}

void QnRtspIoDevice::shutdown()
{
    if (m_transport == RtspTransport::tcp)
        m_owner->shutdown();
    else
        m_mediaSocket->shutdown();
}

void QnRtspIoDevice::updateRemoteMulticastPorts(quint16 mediaPort, quint16 rtcpPort)
{
    if (m_transport != RtspTransport::multicast)
        return;

    m_remoteMediaPort = mediaPort;
    m_remoteRtcpPort = rtcpPort;
    updateSockets();
}

void QnRtspIoDevice::setTransport(RtspTransport rtspTransport)
{
    if (m_transport == rtspTransport)
        return;
    m_transport = rtspTransport;
    updateSockets();
}

void QnRtspIoDevice::processRtcpData()
{
    quint8 rtcpBuffer[MAX_RTCP_PACKET_SIZE];
    quint8 sendBuffer[MAX_RTCP_PACKET_SIZE];

    bool rtcpReportAlreadySent = false;
    while( m_rtcpSocket->hasData() )
    {
        nx::network::SocketAddress senderEndpoint;
        int bytesRead = m_rtcpSocket->recvFrom(rtcpBuffer, sizeof(rtcpBuffer), &senderEndpoint);
        if (bytesRead > 0)
        {
            if (!m_rtcpSocket->isConnected())
            {
                if (!m_rtcpSocket->setDestAddr(senderEndpoint))
                {
                    qWarning() << "QnRtspIoDevice::processRtcpData(): setDestAddr() failed: " << SystemError::getLastOSErrorText();
                }
            }
            nx::streaming::rtp::RtcpSenderReport senderReport;
            if (senderReport.read(rtcpBuffer, bytesRead))
                m_senderReport = senderReport;
            int outBufSize = nx::streaming::rtp::buildClientRtcpReport(sendBuffer, MAX_RTCP_PACKET_SIZE);
            if (outBufSize > 0)
            {
                m_rtcpSocket->send(sendBuffer, outBufSize);
                rtcpReportAlreadySent = true;
            }
        }
    }

    if (m_forceRtcpReports && !rtcpReportAlreadySent)
    {
        if (!m_reportTimerStarted)
        {
            m_reportTimer.start();
            m_reportTimerStarted = true;
        }

        if (m_reportTimer.elapsed() > 5000)
        {
            int outBufSize = nx::streaming::rtp::buildClientRtcpReport(sendBuffer, MAX_RTCP_PACKET_SIZE);
            if (outBufSize > 0)
            {
                auto remoteEndpoint = nx::network::SocketAddress(m_hostAddress, m_remoteRtcpPort);
                if (!m_rtcpSocket->setDestAddr(remoteEndpoint))
                {
                    qWarning()
                        << "RTPIODevice::processRtcpData(): setDestAddr() failed: "
                        << SystemError::getLastOSErrorText();
                }
                m_rtcpSocket->send(sendBuffer, outBufSize);
            }

            m_reportTimer.restart();
        }
    }
}

void QnRtspIoDevice::updateSockets()
{
    if (m_transport == RtspTransport::tcp)
    {
        m_mediaSocket.reset();
        m_rtcpSocket.reset();
        return;
    }

    auto createSocket =
        [this](int port)
        {
            auto result = nx::network::SocketFactory::createDatagramSocket();
            if (m_transport == RtspTransport::multicast)
                result->setReuseAddrFlag(true);

            const auto res = result->bind(nx::network::SocketAddress(nx::network::HostAddress::anyHost, port));
            result->setRecvTimeout(500);
            return result;
        };

    m_mediaSocket =
        createSocket(m_transport == RtspTransport::multicast ? m_remoteMediaPort : 0);
    m_rtcpSocket =
        createSocket(m_transport == RtspTransport::multicast ? m_remoteRtcpPort : 0);
}

static const size_t ADDITIONAL_READ_BUFFER_CAPACITY = 64 * 1024;

//-------------------------------------------------------------------------------------------------
// QnRtspClient

QnRtspClient::QnRtspClient(
    const Config& config,
    std::unique_ptr<nx::network::AbstractStreamSocket> tcpSock)
    :
    m_config(config),
    m_csec(2),
    m_transport(RtspTransport::udp),
    m_selectedAudioChannel(0),
    m_startTime(DATETIME_NOW),
    m_openedTime(AV_NOPTS_VALUE),
    m_endTime(AV_NOPTS_VALUE),
    m_scale(1.0),
    m_tcpTimeout(10 * 1000),
    m_responseCode(nx::network::http::StatusCode::ok),
    m_isAudioEnabled(true),
    m_TimeOut(0),
    m_tcpSock(std::move(tcpSock)),
    m_additionalReadBuffer( nullptr ),
    m_additionalReadBufferPos( 0 ),
    m_additionalReadBufferSize( 0 ),
    m_rtspAuthCtx(config.shouldGuessAuthDigest),
    m_userAgent(nx::network::http::userAgentString()),
    m_defaultAuthScheme(nx::network::http::header::AuthScheme::basic)
{
    m_responseBuffer = new quint8[RTSP_BUFFER_LEN];
    m_responseBufferLen = 0;

    if( !m_tcpSock )
        m_tcpSock = nx::network::SocketFactory::createStreamSocket();

    m_additionalReadBuffer = new char[ADDITIONAL_READ_BUFFER_CAPACITY];
}

QnRtspClient::~QnRtspClient()
{
    stop();
    delete [] m_responseBuffer;

    delete[] m_additionalReadBuffer;
}

nx::network::rtsp::StatusCodeValue QnRtspClient::getLastResponseCode() const
{
    return m_responseCode;
}

void QnRtspClient::parseSDP(const QByteArray& data)
{
    m_sdp.parse(QString(data));

    if (m_sdp.serverAddress.isMulticast())
        m_transport = RtspTransport::multicast;

    for (const auto& media: m_sdp.media)
    {
        if (!media.rtpmap.codecName.isEmpty())
            m_sdpTracks.emplace_back(media, this, m_transport, media.serverPort);
    }

    m_sdpTracks.erase(std::remove_if(
        m_sdpTracks.begin(), m_sdpTracks.end(),
        [this](const QnRtspClient::SDPTrackInfo& track)
    {
        if (m_config.backChannelAudioOnly && track.sdpMedia.mediaType != nx::streaming::Sdp::MediaType::Audio)
            return true; //< Remove non audio tracks for back audio channel.
        return m_config.backChannelAudioOnly != track.sdpMedia.sendOnly;
    }),
        m_sdpTracks.end());
}

void QnRtspClient::parseRangeHeader(const QString& rangeStr)
{
    //TODO use nx::network::rtsp::parseRangeHeader
    QStringList rangeType = rangeStr.trimmed().split(QLatin1Char('='));
    if (rangeType.size() < 2)
        return;
    if (rangeType[0] == QLatin1String("clock"))
    {
        int index = rangeType[1].lastIndexOf(QLatin1Char('-'));
        QString start = rangeType[1].mid(0, index);
        QString end = rangeType[1].mid(index + 1);
        if(start.endsWith(QLatin1Char('-'))) {
            start = start.left(start.size() - 1);
            end = QLatin1Char('-') + end;
        }
        if (start == QLatin1String("now")) {
            m_startTime= DATETIME_NOW;
        }
        else {
            double val = start.toDouble();
            m_startTime = val < 1000000 ? val * 1000000.0 : val;
        }
        if (index > 0)
        {
            if (end == QLatin1String("now")) {
                m_endTime = DATETIME_NOW;
            }
            else {
                double val = end.toDouble();
                m_endTime = val < 1000000 ? val * 1000000.0 : val;
            }
        }
    }
}

void QnRtspClient::updateResponseStatus(const QByteArray& response)
{
    int firstLineEnd = response.indexOf('\n');
    if (firstLineEnd >= 0)
    {
        nx::network::http::StatusLine statusLine;
        statusLine.parse( nx::network::http::ConstBufferRefType(response, 0, firstLineEnd) );
        m_responseCode = statusLine.statusCode;
        m_reasonPhrase = QLatin1String(statusLine.reasonPhrase);
    }
}

CameraDiagnostics::Result QnRtspClient::open(const nx::utils::Url& url, qint64 startTime)
{
    if (startTime != AV_NOPTS_VALUE)
        m_openedTime = startTime;

    m_SessionId.clear();
    m_responseCode = nx::network::http::StatusCode::ok;
    m_url = url;
    m_responseBufferLen = 0;
    m_rtspAuthCtx.clear();
    if (m_defaultAuthScheme == nx::network::http::header::AuthScheme::basic)
    {
        m_rtspAuthCtx.setAuthenticationHeader(
            nx::network::http::header::WWWAuthenticate(m_defaultAuthScheme));
    }

    bool isSslRequired = false;
    const auto urlScheme = m_url.scheme().toLower().toUtf8();
    if (urlScheme == nx::network::rtsp::kUrlSchemeName)
        isSslRequired = false;
    else if (urlScheme == nx::network::rtsp::kSecureUrlSchemeName)
        isSslRequired = true;
    else
        return CameraDiagnostics::UnsupportedProtocolResult(m_url.toString(), m_url.scheme());

    std::unique_ptr<nx::network::AbstractStreamSocket> previousSocketHolder;
    {
        QnMutexLocker lock(&m_socketMutex);
        previousSocketHolder = std::move(m_tcpSock);
        m_tcpSock = nx::network::SocketFactory::createStreamSocket(isSslRequired);
        m_additionalReadBufferSize = 0;
    }

    m_tcpSock->setRecvTimeout(TCP_RECEIVE_TIMEOUT_MS);

    nx::network::SocketAddress targetAddress;
    if (m_proxyAddress)
        targetAddress = *m_proxyAddress;
    else
        targetAddress = nx::network::SocketAddress(m_url.host(), m_url.port(DEFAULT_RTP_PORT));

    if (!m_tcpSock->connect(targetAddress, std::chrono::milliseconds(TCP_CONNECT_TIMEOUT_MS)))
        return CameraDiagnostics::CannotOpenCameraMediaPortResult(url.toString(), targetAddress.port);
    previousSocketHolder.reset(); //< Reset old socket after taking new one to guarantee new TCP port.

    m_tcpSock->setNoDelay(true);

    m_tcpSock->setRecvTimeout(m_tcpTimeout);
    m_tcpSock->setSendTimeout(m_tcpTimeout);

    if (m_playNowMode)
    {
        m_contentBase = m_url.toString();
        return CameraDiagnostics::NoErrorResult();
    }

    QByteArray response;
    if( !sendRequestAndReceiveResponse( createDescribeRequest(), response ) )
    {
        stop();
        return CameraDiagnostics::ConnectionClosedUnexpectedlyResult(url.toString(), targetAddress.port);
    }

    QString tmp = extractRTSPParam(QLatin1String(response), QLatin1String("Range:"));
    if (!tmp.isEmpty())
        parseRangeHeader(tmp);

    CameraDiagnostics::Result result = CameraDiagnostics::NoErrorResult();
    updateResponseStatus(response);
    switch( m_responseCode )
    {
        case nx::network::http::StatusCode::ok:
            break;
        case nx::network::http::StatusCode::unauthorized:
        case nx::network::http::StatusCode::proxyAuthenticationRequired:
            stop();
            return CameraDiagnostics::NotAuthorisedResult(url.toString());
        default:
            stop();
            return CameraDiagnostics::RequestFailedResult( lit("DESCRIBE %1").arg(url.toString()), m_reasonPhrase );
    }

    int sdp_index = response.indexOf(QLatin1String("\r\n\r\n"));

    if (sdp_index  < 0 || sdp_index+4 >= response.size()) {
        stop();
        return CameraDiagnostics::NoMediaTrackResult(url.toString());
    }

    parseSDP(response.mid(sdp_index + 4));

    if (m_sdpTracks.size() <= 0)
    {
        stop();
        result = CameraDiagnostics::NoMediaTrackResult(url.toString());
    }

    /*
     * RFC2326
     * 1. The RTSP Content-Base field.
     * 2. The RTSP Content-Location field.
     * 3. The RTSP request URL.
     * If this attribute contains only an asterisk (*), then the URL is
     * treated as if it were an empty embedded URL, and thus inherits the
     * entire base URL.
    */
    m_contentBase = extractRTSPParam(QLatin1String(response), QLatin1String("Content-Base:"));
    if (m_contentBase.isEmpty())
        m_contentBase = extractRTSPParam(QLatin1String(response), QLatin1String("Content-Location:"));
    if (m_contentBase.isEmpty())
        m_contentBase = m_url.toString(); // TODO remove url params?

    if (result)
        NX_DEBUG(this, "Sucessfully opened RTSP stream %1", m_url);

    return result;
}

bool QnRtspClient::play(qint64 positionStart, qint64 positionEnd, double scale)
{
    m_prefferedTransport = m_transport;
    if (m_prefferedTransport == RtspTransport::notDefined)
        m_prefferedTransport = RtspTransport::tcp;
    m_TimeOut = 0; // default timeout 0 ( do not send keep alive )
    if (!m_playNowMode) {
        if (!sendSetup() || m_sdpTracks.empty())
            return false;
    }

    if (!sendPlay(positionStart, positionEnd, scale))
    {
        m_sdpTracks.clear();
        return false;
    }

    return true;
}

bool QnRtspClient::stop()
{
    QnMutexLocker lock(&m_socketMutex);
    m_tcpSock->close();
    return true;
}

void QnRtspClient::shutdown()
{
    QnMutexLocker lock(&m_socketMutex);
    m_tcpSock->shutdown();
}

bool QnRtspClient::isOpened() const
{
    QnMutexLocker lock(&m_socketMutex);
    return m_tcpSock->isConnected();
}

unsigned int QnRtspClient::sessionTimeoutMs()
{
    return m_TimeOut;
}

const nx::streaming::Sdp& QnRtspClient::getSdp() const
{
    return m_sdp;
}

void QnRtspClient::addAuth( nx::network::http::Request* const request )
{
    QnClientAuthHelper::addAuthorizationToRequest(
        m_auth,
        request,
        &m_rtspAuthCtx );   //ignoring result
}

void QnRtspClient::addCommonHeaders(nx::network::http::HttpHeaders& headers)
{
    nx::network::http::insertOrReplaceHeader(
        &headers, nx::network::http::HttpHeader("CSeq", QByteArray::number(m_csec++)));
    nx::network::http::insertOrReplaceHeader(
        &headers, nx::network::http::HttpHeader("User-Agent", m_userAgent));
    nx::network::http::insertOrReplaceHeader(
        &headers,
        nx::network::http::HttpHeader(
            "Host",
            nx::network::url::getEndpoint(m_url).toString().toUtf8()));
}

void QnRtspClient::addAdditionalHeaders(
    const QString& requestName,
    nx::network::http::HttpHeaders* outHeaders)
{
    for (const auto& header: m_additionalHeaders[requestName])
        nx::network::http::insertOrReplaceHeader(outHeaders, header);
}

nx::network::http::Request QnRtspClient::createDescribeRequest()
{
    m_sdpTracks.clear();

    nx::network::http::Request request;
    request.requestLine.method = kDescribeCommand;
    request.requestLine.url = m_url;
    request.requestLine.version = nx::network::rtsp::rtsp_1_0;
    addCommonHeaders(request.headers);
    request.headers.insert( nx::network::http::HttpHeader( "Accept", "application/sdp" ) );
    if( m_openedTime != AV_NOPTS_VALUE )
        addRangeHeader( &request, m_openedTime, AV_NOPTS_VALUE );
    addAdditionAttrs( &request );
    return request;
}

bool QnRtspClient::sendRequestInternal(nx::network::http::Request&& request)
{
    addAuth(&request);
    addAdditionAttrs(&request);
    QByteArray requestBuf;
    request.serialize( &requestBuf );
    return m_tcpSock->send(requestBuf.constData(), requestBuf.size()) > 0;
}

bool QnRtspClient::sendDescribe()
{
    return sendRequestInternal(createDescribeRequest());
}

bool QnRtspClient::sendOptions()
{
    nx::network::http::Request request;
    request.requestLine.method = kOptionsCommand;
    request.requestLine.url = m_url;
    request.requestLine.version = nx::network::rtsp::rtsp_1_0;
    addCommonHeaders(request.headers);
    return sendRequestInternal(std::move(request));
}

int QnRtspClient::getTrackCount(nx::streaming::Sdp::MediaType mediaType) const
{
    int result = 0;
    for (const auto& track: m_sdpTracks)
    {
        if (track.sdpMedia.mediaType == mediaType)
            ++result;
    }
    return result;
}

QStringList QnRtspClient::getSdpByType(nx::streaming::Sdp::MediaType mediaType) const
{
    for (const auto& track: m_sdpTracks)
    {
        if (track.sdpMedia.mediaType == mediaType)
            return track.sdpMedia.sdpAttributes;
    }
    return QStringList();
}

void QnRtspClient::registerRTPChannel(int rtpNum, int rtcpNum, int trackIndex)
{
    m_rtpToTrack.resize(std::max(rtpNum, rtcpNum) + 1);
    m_rtpToTrack[rtpNum].trackIndex = trackIndex;
    m_rtpToTrack[rtcpNum].trackIndex = trackIndex;
    m_rtpToTrack[rtcpNum].isRtcp = true;
}

bool QnRtspClient::sendSetup()
{
    int audioNum = 0;
    for (uint32_t i = 0; i < m_sdpTracks.size(); ++i)
    {
        auto& track = m_sdpTracks[i];
        if (track.sdpMedia.mediaType == nx::streaming::Sdp::MediaType::Audio)
        {
            if (!m_isAudioEnabled || audioNum++ != m_selectedAudioChannel)
                continue;
        }
        else if (track.sdpMedia.mediaType != nx::streaming::Sdp::MediaType::Video &&
            track.sdpMedia.rtpmap.codecName != METADATA_STR)
        {
            continue; // skip unknown metadata e.t.c
        }
        nx::network::http::Request request;
        auto setupUrl = track.sdpMedia.control == "*"
                ? nx::utils::Url()
                : nx::utils::Url(track.sdpMedia.control);

        request.requestLine.method = kSetupCommand;
        if( setupUrl.isRelative() )
        {
            // SETUP postfix should be writen after url query params. It's invalid url, but it's required according to RTSP standard
            request.requestLine.url = m_contentBase
                    + ((m_contentBase.endsWith(lit("/")) || setupUrl.isEmpty()) ? lit("") : lit("/"))
                    + setupUrl.toString();
        }
        else
        {
            // full track url in a prefix
            request.requestLine.url = setupUrl;
        }

        request.requestLine.version = nx::network::rtsp::rtsp_1_0;
        addCommonHeaders(request.headers);

        {   //generating transport header
            nx::network::http::StringType transportStr = "RTP/AVP/";
            transportStr += m_prefferedTransport == RtspTransport::tcp ? "TCP" : "UDP";
            transportStr += m_prefferedTransport == RtspTransport::multicast ? ";multicast;" : ";unicast;";

            track.ioDevice->setTransport(m_prefferedTransport);
            if (m_prefferedTransport == RtspTransport::multicast)
            {
                track.ioDevice->bindToMulticastAddress(m_sdp.serverAddress,
                    m_tcpSock->getLocalAddress().address.toString());
            }

            if (m_prefferedTransport != RtspTransport::tcp)
            {
                transportStr += m_prefferedTransport == RtspTransport::multicast ? "port=" : "client_port=";
                transportStr += QString::number(track.ioDevice->getMediaSocket()->getLocalAddress().port);
                transportStr += '-';
                transportStr += QString::number(track.ioDevice->getRtcpSocket()->getLocalAddress().port);
            }
            else
            {
                track.interleaved = QPair<int,int>(i * SDP_TRACK_STEP, i * SDP_TRACK_STEP + 1);
                transportStr += QLatin1String("interleaved=") + QString::number(track.interleaved.first) + QLatin1Char('-') + QString::number(track.interleaved.second);
            }
            request.headers.insert(nx::network::http::HttpHeader("Transport", transportStr));
        }

        if(!m_SessionId.isEmpty())
            request.headers.insert(nx::network::http::HttpHeader("Session", m_SessionId.toLatin1()));

        QByteArray responce;
        if (!sendRequestAndReceiveResponse(std::move(request), responce))
            return false;

        if (!responce.startsWith("RTSP/1.0 200"))
        {
            if (m_transport == RtspTransport::tcp && m_prefferedTransport == RtspTransport::tcp)
            {
                m_prefferedTransport = RtspTransport::udp;
                if (!sendSetup()) //< Try UDP transport.
                    return false;
            }
            else
                return false;
        }

        QString sessionParam = extractRTSPParam(QLatin1String(responce), QLatin1String("Session:"));
        if (sessionParam.size() > 0)
        {
            QStringList tmpList = sessionParam.split(QLatin1Char(';'));
            m_SessionId = tmpList[0];

            for (int i = 0; i < tmpList.size(); ++i)
            {
                tmpList[i] = tmpList[i].trimmed().toLower();
                if (tmpList[i].startsWith(QLatin1String("timeout")))
                {
                    QStringList tmpParams = tmpList[i].split(QLatin1Char('='));
                    if (tmpParams.size() > 1) {
                        m_TimeOut = tmpParams[1].toInt();
                        if (m_TimeOut > 0 && m_TimeOut < 5000)
                            m_TimeOut *= 1000; // convert seconds to ms
                    }
                }
            }
        }

        QString transportParam = extractRTSPParam(QLatin1String(responce), QLatin1String("Transport:"));
        if (transportParam.size() > 0)
        {
            QStringList tmpList = transportParam.split(QLatin1Char(';'));
            for (int k = 0; k < tmpList.size(); ++k)
            {
                tmpList[k] = tmpList[k].trimmed().toLower();
                if (tmpList[k].startsWith(QLatin1String("ssrc")))
                {
                    QStringList tmpParams = tmpList[k].split(QLatin1Char('='));
                    if (tmpParams.size() > 1) {
                        bool ok;
                        track.ioDevice->setSSRC((quint32)tmpParams[1].toLongLong(&ok, 16));
                    }
                }
                else if (tmpList[k].startsWith(QLatin1String("interleaved"))) {
                    QStringList tmpParams = tmpList[k].split(QLatin1Char('='));
                    if (tmpParams.size() > 1) {
                        tmpParams = tmpParams[1].split(QLatin1String("-"));
                        if (tmpParams.size() == 2) {
                            track.interleaved =
                                QPair<int,int>(tmpParams[0].toInt(), tmpParams[1].toInt());
                            registerRTPChannel(track.interleaved.first, track.interleaved.second, i);
                        }
                    }
                }
                else if (tmpList[k].startsWith(QLatin1String("server_port"))) {
                    QStringList tmpParams = tmpList[k].split(QLatin1Char('='));
                    if (tmpParams.size() > 1) {
                        tmpParams = tmpParams[1].split(QLatin1String("-"));
                        if (tmpParams.size() == 2) {
                            track.setRemoteEndpointRtcpPort(tmpParams[1].toInt());
                        }
                    }
                }
                else if (m_transport == RtspTransport::multicast && tmpList[k].startsWith("port"))
                {
                    const auto ports = parsePorts(tmpList[k]);
                    if (ports.mediaPort && ports.rtcpPort)
                    {
                        track.ioDevice->updateRemoteMulticastPorts(
                            ports.mediaPort, ports.rtcpPort);

                        track.ioDevice->bindToMulticastAddress(m_sdp.serverAddress,
                            m_tcpSock->getLocalAddress().address.toString());
                    }
                }
            }
        }
        updateTransportHeader(responce);
    }
    return true;
}

void QnRtspClient::addAdditionAttrs( nx::network::http::Request* const request )
{
    for (QMap<QByteArray, QByteArray>::const_iterator i = m_additionAttrs.begin(); i != m_additionAttrs.end(); ++i)
        nx::network::http::insertOrReplaceHeader(
            &request->headers,
            nx::network::http::HttpHeader(i.key(), i.value()));
}

bool QnRtspClient::sendSetParameter( const QByteArray& paramName, const QByteArray& paramValue )
{
    nx::network::http::Request request;

    request.messageBody.append(paramName);
    request.messageBody.append(": ");
    request.messageBody.append(paramValue);
    request.messageBody.append("\r\n");

    request.requestLine.method = kSetParameterCommand;
    request.requestLine.url = m_url;
    request.requestLine.version = nx::network::rtsp::rtsp_1_0;
    addCommonHeaders(request.headers);
    request.headers.insert( nx::network::http::HttpHeader( "Session", m_SessionId.toLatin1() ) );
    request.headers.insert( nx::network::http::HttpHeader( "Content-Length", QByteArray::number(request.messageBody.size()) ) );
    return sendRequestInternal(std::move(request));
}

QByteArray QnRtspClient::nptPosToString(qint64 posUsec) const
{
    switch (m_dateTimeFormat)
    {
        case DateTimeFormat::ISO:
        {
            auto datetime = QDateTime::fromMSecsSinceEpoch(posUsec / 1000, Qt::UTC);
            return datetime.toString(lit("yyyyMMddThhmmssZ")).toLocal8Bit();
        }
        default:
            return nx::network::http::StringType::number(posUsec);
    }
}

void QnRtspClient::addRangeHeader( nx::network::http::Request* const request, qint64 startPos, qint64 endPos )
{
    nx::network::http::StringType rangeVal;
    if (startPos != qint64(AV_NOPTS_VALUE))
    {
        //There is no guarantee that every RTSP server understands utc ranges.
        //RFC requires only npt ranges support.
        if (startPos != DATETIME_NOW)
            rangeVal += "clock=" + nptPosToString(startPos);
        else
            rangeVal += "clock=now";
        rangeVal += '-';
        if (endPos != qint64(AV_NOPTS_VALUE))
        {
            if (endPos != DATETIME_NOW)
                rangeVal += nptPosToString(endPos);
            else
                rangeVal += "clock";
        }

        nx::network::http::insertOrReplaceHeader(
            &request->headers,
            nx::network::http::HttpHeader("Range", rangeVal));
    }
}

QByteArray QnRtspClient::getGuid()
{
    QnMutexLocker lock( &m_guidMutex );
    if (m_guid.isEmpty())
        m_guid = QnUuid::createUuid().toString().toUtf8();
    return m_guid;
}

nx::network::http::Request QnRtspClient::createPlayRequest( qint64 startPos, qint64 endPos )
{
    nx::network::http::Request request;
    request.requestLine.method = kPlayCommand;
    request.requestLine.url = m_contentBase;
    request.requestLine.version = nx::network::rtsp::rtsp_1_0;
    addCommonHeaders(request.headers);
    request.headers.insert( nx::network::http::HttpHeader( "Session", m_SessionId.toLatin1() ) );
    addRangeHeader( &request, startPos, endPos );
    addAdditionalHeaders(kPlayCommand, &request.headers);
    request.headers.insert( nx::network::http::HttpHeader( "Scale", QByteArray::number(m_scale)) );
    if (m_playNowMode)
    {
        nx::network::http::insertOrReplaceHeader(
            &request.headers,
            nx::network::http::HttpHeader("x-play-now", "true"));
        nx::network::http::insertOrReplaceHeader(
            &request.headers,
            nx::network::http::HttpHeader(Qn::GUID_HEADER_NAME, getGuid()));
    }
    return request;
}

bool QnRtspClient::sendPlay(qint64 startPos, qint64 endPos, double scale)
{
    QByteArray response;

    m_scale = scale;

    nx::network::http::Request request = createPlayRequest( startPos, endPos );
    if( !sendRequestAndReceiveResponse( std::move(request), response ) )
    {
        stop();
        return false;
    }

    while (1)
    {
        QString cseq = extractRTSPParam(QLatin1String(response), QLatin1String("CSeq:"));
        if (cseq.toInt() == (int)m_csec-1)
            break;
        else if (!readTextResponce(response))
            return false; // try next response
    }

    QString tmp = extractRTSPParam(QLatin1String(response), QLatin1String("Range:"));
    if (!tmp.isEmpty())
        parseRangeHeader(tmp);

    tmp = extractRTSPParam(QLatin1String(response), QLatin1String("x-video-layout:"));
    if (!tmp.isEmpty())
        m_videoLayout = tmp; // refactor here: add all attributes to list

    if (response.startsWith("RTSP/1.0 200"))
    {
        updateTransportHeader(response);
        m_keepAliveTime.restart();
        return true;
    }

    return false;

}

bool QnRtspClient::sendPause()
{
    nx::network::http::Request request;
    request.requestLine.method = kPauseCommand;
    request.requestLine.url = m_url;
    request.requestLine.version = nx::network::rtsp::rtsp_1_0;
    addCommonHeaders(request.headers);
    request.headers.insert( nx::network::http::HttpHeader( "Session", m_SessionId.toLatin1() ) );
    return sendRequestInternal(std::move(request));
}

bool QnRtspClient::sendTeardown()
{
    nx::network::http::Request request;
    request.requestLine.method = kTeardownCommand;
    request.requestLine.url = m_url;
    request.requestLine.version = nx::network::rtsp::rtsp_1_0;
    addCommonHeaders(request.headers);
    request.headers.insert( nx::network::http::HttpHeader( "Session", m_SessionId.toLatin1() ) );
    return sendRequestInternal(std::move(request));
}

bool QnRtspClient::sendKeepAliveIfNeeded()
{
    // send rtsp keep alive
    if (m_TimeOut==0)
        return true;

    if (m_keepAliveTime.elapsed() < (int) m_TimeOut - RESERVED_TIMEOUT_TIME)
        return true;
    else
    {
        bool res= sendKeepAlive();
        m_keepAliveTime.restart();
        return res;
    }
}

bool QnRtspClient::sendKeepAlive()
{
    nx::network::http::Request request;
    request.requestLine.method = kGetParameterCommand;
    request.requestLine.url = m_url;
    request.requestLine.version = nx::network::rtsp::rtsp_1_0;
    addCommonHeaders(request.headers);
    request.headers.insert( nx::network::http::HttpHeader( "Session", m_SessionId.toLatin1() ) );
    return sendRequestInternal(std::move(request));
}

void QnRtspClient::sendBynaryResponse(const quint8* buffer, int size)
{
    m_tcpSock->send(buffer, size);
}

bool QnRtspClient::processTextResponseInsideBinData()
{
    // have text response or part of text response.
    int bytesRead = readSocketWithBuffering(m_responseBuffer+m_responseBufferLen, qMin(1024, RTSP_BUFFER_LEN - m_responseBufferLen), true);
    if (bytesRead <= 0)
        return false;
    m_responseBufferLen += bytesRead;

    quint8* curPtr = m_responseBuffer;
    quint8* bEnd = m_responseBuffer+m_responseBufferLen;
    for(; curPtr < bEnd && *curPtr != '$'; curPtr++);

    bool isReadyToProcess = curPtr < bEnd;
    if (!isReadyToProcess)
    {
        const auto messageSize = QnTCPConnectionProcessor::isFullMessage(
            QByteArray::fromRawData((const char*)m_responseBuffer, m_responseBufferLen));

        if (messageSize < 0)
            return false;

        if (messageSize > 0)
            isReadyToProcess = true;
    }

    if (isReadyToProcess)
    {
        QByteArray textResponse;
        textResponse.append((const char*) m_responseBuffer, curPtr - m_responseBuffer);
        memmove(m_responseBuffer, curPtr, bEnd - curPtr);
        m_responseBufferLen = bEnd - curPtr;
        QString tmp = extractRTSPParam(QLatin1String(textResponse), QLatin1String("Range:"));
        if (!tmp.isEmpty())
            parseRangeHeader(tmp);
    }

    return true;
}

int QnRtspClient::readBinaryResponce(quint8* data, int maxDataSize)
{
    while (m_tcpSock->isConnected())
    {
        while (m_responseBufferLen < 4) {
            int bytesRead = readSocketWithBuffering(m_responseBuffer+m_responseBufferLen, 4 - m_responseBufferLen, true);
            if (bytesRead <= 0)
                return bytesRead;
            m_responseBufferLen += bytesRead;
        }
        if (m_responseBuffer[0] == '$')
            break;

        // have text response or part of text response.
        if (!processTextResponseInsideBinData())
            return -1;
    }
    int dataLen = (m_responseBuffer[2]<<8) + m_responseBuffer[3] + 4;
    if (maxDataSize < dataLen)
        return -2; // not enough buffer
    int copyLen = qMin(dataLen, m_responseBufferLen);
    memcpy(data, m_responseBuffer, copyLen);
    if (m_responseBufferLen > copyLen)
        memmove(m_responseBuffer, m_responseBuffer + copyLen, m_responseBufferLen - copyLen);
    data += copyLen;
    m_responseBufferLen -= copyLen;
    for (int dataRestLen = dataLen - copyLen; dataRestLen > 0;)
    {
        int bytesRead = readSocketWithBuffering(data, dataRestLen, true);
        if (bytesRead <= 0)
            return bytesRead;
        dataRestLen -= bytesRead;
        data += bytesRead;
    }
    return dataLen;
}

quint8* QnRtspClient::prepareDemuxedData(std::vector<QnByteArray*>& demuxedData, int channel, int reserve)
{
    if (channel >= 0 && demuxedData.size() <= (size_t)channel)
        demuxedData.resize(channel+1);
    if (demuxedData[channel] == 0)
        demuxedData[channel] = new QnByteArray(16, 32);
    QnByteArray* dataVect = demuxedData[channel];
    //dataVect->resize(dataVect->size() + reserve);
    dataVect->reserve(dataVect->size() + reserve);
    return (quint8*) dataVect->data() + dataVect->size();
}

int QnRtspClient::readBinaryResponce(std::vector<QnByteArray*>& demuxedData, int& channelNumber)
{
    while (m_tcpSock->isConnected())
    {
        while (m_responseBufferLen < 4) {
            int bytesRead = readSocketWithBuffering(m_responseBuffer+m_responseBufferLen, 4 - m_responseBufferLen, true);
            if (bytesRead <= 0)
                return bytesRead;
            m_responseBufferLen += bytesRead;
        }
        if (m_responseBuffer[0] == '$')
            break;
        if (!processTextResponseInsideBinData())
            return -1;
    }

    int dataLen = (m_responseBuffer[2]<<8) + m_responseBuffer[3] + 4;
    int copyLen = qMin(dataLen, m_responseBufferLen);
    channelNumber = m_responseBuffer[1];
    quint8* data = prepareDemuxedData(demuxedData, channelNumber, dataLen);
    //quint8* data = (quint8*) dataVect->data() + dataVect->size() - dataLen;

    memcpy(data, m_responseBuffer, copyLen);
    if (m_responseBufferLen > copyLen)
        memmove(m_responseBuffer, m_responseBuffer + copyLen, m_responseBufferLen - copyLen);
    data += copyLen;
    m_responseBufferLen -= copyLen;

    for (int dataRestLen = dataLen - copyLen; dataRestLen > 0;)
    {
        int bytesRead = readSocketWithBuffering(data, dataRestLen, true);
        if (bytesRead <= 0)
            return bytesRead;
        dataRestLen -= bytesRead;
        data += bytesRead;
    }

    demuxedData[channelNumber]->finishWriting(dataLen);
    return dataLen;
}

bool QnRtspClient::processTcpRtcpData(const quint8* data, int size)
{
    if (size < 4 || data[0] != '$')
        return false;
    int rtpChannelNum = data[1];
    int trackNum = getTrackNum(rtpChannelNum);
    if (trackNum >= (int)m_sdpTracks.size())
        return false;
    QnRtspIoDevice* ioDevice = m_sdpTracks[trackNum].ioDevice.get();
    if (!ioDevice)
        return false;

    nx::streaming::rtp::RtcpSenderReport senderReport;
    if (senderReport.read(data + 4, size - 4))
    {
        if (ioDevice->getSSRC() == 0 || ioDevice->getSSRC() == senderReport.ssrc)
            ioDevice->setSenderReport(senderReport);
    }
    return true;
}

// demux text data only
bool QnRtspClient::readTextResponce(QByteArray& response)
{
    int ignoreDataSize = 0;
    bool needMoreData = m_responseBufferLen == 0;
    for (int i = 0; i < 1000 && ignoreDataSize < 1024*1024*3 && m_tcpSock->isConnected(); ++i)
    {
        if (needMoreData) {
            int bytesRead = readSocketWithBuffering(m_responseBuffer + m_responseBufferLen, qMin(1024, RTSP_BUFFER_LEN - m_responseBufferLen), true);
            if (bytesRead <= 0)
            {
                if( bytesRead == 0 )
                {
                    NX_INFO(this, lit("RTSP connection to %1 has been unexpectedly closed").
                        arg(m_tcpSock->getForeignAddress().toString()));
                }
                else if (!m_tcpSock->isClosed())
                {
                    NX_WARNING(this, lit("Error reading RTSP response from %1. %2").
                        arg(m_tcpSock->getForeignAddress().toString()).arg(SystemError::getLastOSErrorText()));
                }
                return false; //< error occurred or connection closed
            }
            m_responseBufferLen += bytesRead;
        }
        if (m_responseBuffer[0] == '$') {
            // binary data
            quint8 tmpData[1024*64];
            int bytesRead = readBinaryResponce(tmpData, sizeof(tmpData)); // skip binary data

            int rtpChannelNum = tmpData[1];
            if (isRtcp(rtpChannelNum))
            {
                if (!processTcpRtcpData(tmpData, bytesRead))
                    NX_VERBOSE(this, "Can't parse RTCP report while reading text response");
            }

            int oldIgnoreDataSize = ignoreDataSize;
            ignoreDataSize += bytesRead;
            if (oldIgnoreDataSize / 64000 != ignoreDataSize/64000)
                QnSleep::msleep(1);
            needMoreData = m_responseBufferLen == 0;
        }
        else
        {
            // text data
            int msgLen = QnTCPConnectionProcessor::isFullMessage(QByteArray::fromRawData((const char*)m_responseBuffer, m_responseBufferLen));
            if (msgLen < 0)
                return false;

            if (msgLen > 0)
            {
                response = QByteArray((const char*) m_responseBuffer, msgLen);
                memmove(m_responseBuffer, m_responseBuffer + msgLen, m_responseBufferLen - msgLen);
                m_responseBufferLen -= msgLen;
                return true;
            }
            needMoreData = true;
        }
        if (m_responseBufferLen == RTSP_BUFFER_LEN)
        {
            NX_INFO(this, lit("RTSP response from %1 has exceeded max response size (%2)").
                arg(m_tcpSock->getForeignAddress().toString()).arg(RTSP_BUFFER_LEN));
            return false;
        }
    }
    return false;
}

QString QnRtspClient::extractRTSPParam(const QString& buffer, const QString& paramName)
{
    int pos = buffer.indexOf(paramName, 0, Qt::CaseInsensitive);
    if (pos > 0)
    {
        QString rez;
        bool first = true;
        for (int i = pos + paramName.size() + 1; i < buffer.size(); i++)
        {
            if (buffer[i] == QLatin1Char(' ') && first)
                continue;
            else if (buffer[i] == QLatin1Char('\r') || buffer[i] == QLatin1Char('\n'))
                break;
            else {
                rez += buffer[i];
                first = false;
            }
        }
        return rez;
    }
    else
        return QString();
}

void QnRtspClient::updateTransportHeader(QByteArray& responce)
{
    QString tmp = extractRTSPParam(QLatin1String(responce), QLatin1String("Transport:"));
    if (tmp.size() > 0)
    {
        QStringList tmpList = tmp.split(QLatin1Char(';'));
        for (int i = 0; i < tmpList.size(); ++i)
        {
            if (tmpList[i].startsWith(QLatin1String("port")))
            {
                QStringList tmpParams = tmpList[i].split(QLatin1Char('='));
                if (tmpParams.size() > 1)
                    m_ServerPort = tmpParams[1].toInt();
            }
        }
    }
}

void QnRtspClient::setTransport(RtspTransport transport)
{
    m_transport = transport;
}

QString QnRtspClient::getTrackCodec(int rtpChannelNum)
{
    if (rtpChannelNum < (int)m_rtpToTrack.size())
        return m_sdpTracks[m_rtpToTrack[rtpChannelNum].trackIndex].sdpMedia.rtpmap.codecName;

    return QString();
}

bool QnRtspClient::isRtcp(int rtpChannelNum)
{
    if (rtpChannelNum < (int)m_rtpToTrack.size())
        return m_rtpToTrack[rtpChannelNum].isRtcp;
    return false;
}

int QnRtspClient::getTrackNum(int rtpChannelNum)
{
    if (rtpChannelNum < (int)m_rtpToTrack.size())
        return m_rtpToTrack[rtpChannelNum].trackIndex;
    return 0;
}

qint64 QnRtspClient::startTime() const
{
    return m_startTime;
}

qint64 QnRtspClient::endTime() const
{
    return m_endTime;
}

float QnRtspClient::getScale() const
{
    return m_scale;
}

void QnRtspClient::setScale(float value)
{
    m_scale = value;
}

void QnRtspClient::setAdditionAttribute(const QByteArray& name, const QByteArray& value)
{
    m_additionAttrs.insert(name, value);
}

void QnRtspClient::removeAdditionAttribute(const QByteArray& name)
{
    m_additionAttrs.remove(name);
}

void QnRtspClient::setTCPTimeout(std::chrono::milliseconds timeout)
{
    m_tcpTimeout = timeout;
    if (m_tcpSock)
    {
        m_tcpSock->setRecvTimeout(m_tcpTimeout);
        m_tcpSock->setSendTimeout(m_tcpTimeout);
    }
}

std::chrono::milliseconds QnRtspClient::getTCPTimeout() const
{
    return m_tcpTimeout;
}

void QnRtspClient::setAuth(const QAuthenticator& auth, nx::network::http::header::AuthScheme::Value defaultAuthScheme)
{
    m_auth = auth;
    m_defaultAuthScheme = defaultAuthScheme;
}

QAuthenticator QnRtspClient::getAuth() const
{
    return m_auth;
}

nx::utils::Url QnRtspClient::getUrl() const
{
    return m_url;
}

void QnRtspClient::setAudioEnabled(bool value)
{
    m_isAudioEnabled = value;
}

bool QnRtspClient::isAudioEnabled() const
{
    return m_isAudioEnabled;
}

void QnRtspClient::setProxyAddr(const QString& addr, int port)
{
    m_proxyAddress = nx::network::SocketAddress(addr, (uint16_t) port);
}

void QnRtspClient::setPlayNowModeAllowed(bool value)
{
    m_playNowMode = value;
}

void QnRtspClient::setUserAgent(const QString& value)
{
    m_userAgent = value.toUtf8();
}

bool QnRtspClient::setTCPReadBufferSize(int value)
{
    return m_tcpSock->setRecvBufferSize(value);
}

QString QnRtspClient::getVideoLayout() const
{
    return m_videoLayout;
}

#ifdef __arm__
void sleepIfEmptySocket(nx::network::AbstractStreamSocket* socket)
{
    static const size_t MS_PER_SEC = 1000;
    static const size_t MAX_BITRATE_BITS_PER_SECOND = 50*1024*1024;
    static const size_t MAX_BITRATE_BYTES_PER_SECOND = MAX_BITRATE_BITS_PER_SECOND / CHAR_BIT;
    static_assert(
        MAX_BITRATE_BYTES_PER_SECOND > ADDITIONAL_READ_BUFFER_CAPACITY * 10,
        "MAX_BITRATE_BYTES_PER_SECOND MUST be 10 times greater than ADDITIONAL_READ_BUFFER_CAPACITY");

    //TODO #ak Better to find other solution and remove this code.
    //At least, move ioctl call to sockets, since socket->handle() is deprecated method
    //(with nat traversal introduced, not every socket will have handle)
    int bytesAv = 0;
    if ((ioctl(socket->handle(), FIONREAD, &bytesAv) == 0) && //socket read buffer size is unknown to us
        bytesAv == 0)    //socket read buffer is empty
    {
        //This sleep somehow reduces CPU time spent by process in kernel space on arm platform
        //Possibly, it is workaround of some other bug somewhere else
        //This code works only on Raspberry and NX1
        QThread::msleep(MS_PER_SEC / (MAX_BITRATE_BYTES_PER_SECOND / ADDITIONAL_READ_BUFFER_CAPACITY));
    }
}
#endif

int QnRtspClient::readSocketWithBuffering(quint8* buf, size_t bufSize, bool readSome)
{
#ifdef __arm__
    sleepIfEmptySocket(m_tcpSock.get());
#endif
    int bytesRead = m_tcpSock->recv(buf, (unsigned int) bufSize, readSome ? 0 : MSG_WAITALL);
    if (bytesRead > 0)
        m_lastReceivedDataTimer.restart();
    return bytesRead;
}

bool QnRtspClient::sendRequestAndReceiveResponse( nx::network::http::Request&& request, QByteArray& responseBuf )
{
    nx::network::rtsp::StatusCodeValue prevStatusCode = nx::network::http::StatusCode::ok;
    addAuth( &request );
    addAdditionAttrs( &request );

    NX_VERBOSE(this, "Send: %1", request.requestLine.toString());
    for( int i = 0; i < 3; ++i )    //needed to avoid infinite loop in case of incorrect server behavour
    {
        QByteArray requestBuf;
        request.serialize( &requestBuf );
        if( m_tcpSock->send(requestBuf.constData(), requestBuf.size()) <= 0 )
        {
            NX_VERBOSE(this, "Failed to send request: %2", SystemError::getLastOSErrorText());
            return false;
        }

        if( !readTextResponce(responseBuf) )
        {
            NX_VERBOSE(this, "Failed to read response");
            return false;
        }

        nx::network::rtsp::RtspResponse response;
        if( !response.parse( responseBuf ) )
        {
            NX_VERBOSE(this, "Failed to parse response");
            return false;
        }

        m_responseCode = response.statusLine.statusCode;
        switch (m_responseCode)
        {
            case nx::network::http::StatusCode::unauthorized:
            case nx::network::http::StatusCode::proxyAuthenticationRequired:
                if( prevStatusCode == m_responseCode )
                {
                    NX_VERBOSE(this, "Already tried authentication and have been rejected");
                    return false;
                }

                prevStatusCode = m_responseCode;
                break;

            default:
                m_serverInfo = nx::network::http::getHeaderValue(response.headers, nx::network::http::header::Server::NAME);
                NX_VERBOSE(this, "Response: %1", response.statusLine.toString());
                return true;
        }

        addCommonHeaders(request.headers); //< Update sequence after unauthorized response.
        const auto authResult = QnClientAuthHelper::authenticate(
            m_auth, response, &request, &m_rtspAuthCtx);
        if (authResult != Qn::Auth_OK)
        {
            NX_VERBOSE(this, "Authentification failed: %1", authResult);
            return false;
        }
    }

    NX_VERBOSE(this, "Response after last retry: %1", prevStatusCode);
    return false;
}

QByteArray QnRtspClient::serverInfo() const
{
    return m_serverInfo;
}

const std::vector<QnRtspClient::SDPTrackInfo>& QnRtspClient::getTrackInfo() const
{
    return m_sdpTracks;
}

nx::network::AbstractStreamSocket* QnRtspClient::tcpSock()
{
    return m_tcpSock.get();
}

void QnRtspClient::setDateTimeFormat(const DateTimeFormat& format)
{
    m_dateTimeFormat = format;
}

void QnRtspClient::addRequestHeader(const QString& requestName, const nx::network::http::HttpHeader& header)
{
    nx::network::http::insertOrReplaceHeader(&m_additionalHeaders[requestName], header);
}

QElapsedTimer QnRtspClient::lastReceivedDataTimer() const
{
    return m_lastReceivedDataTimer;
}
