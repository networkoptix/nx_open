// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtCore/QtGlobal>

#if defined(Q_OS_LINUX)
    #include <sys/ioctl.h>
#endif

#include <nx/streaming/rtsp_client.h>
#include <nx/utils/datetime.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_client.h>
#include <nx/network/rtsp/rtsp_types.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/uuid.h>

#include <nx/vms/auth/time_based_nonce_provider.h>

#include <network/tcp_connection_priv.h>
#include <network/tcp_connection_processor.h>
#include <utils/common/sleep.h>
#include <nx/network/nettools.h>
#include <utils/common/synctime.h>

#define DEFAULT_RTP_PORT 554

static const int TCP_RECEIVE_TIMEOUT_MS = 1000 * 5;
static const int TCP_CONNECT_TIMEOUT_MS = 1000 * 5;
static const int SDP_TRACK_STEP = 2;

using namespace std::chrono;

namespace {

const float kKeepAliveGuardInterval = 0.8f;
const QString METADATA_STR(lit("ffmpeg-metadata"));

auto splitKeyValue(const QString& nameAndValue, QChar delimiter)
{
    QString key = nameAndValue;
    QString value;
    if (int position = nameAndValue.indexOf(delimiter); position >= 0)
    {
        key = nameAndValue.left(position);
        value = nameAndValue.mid(position + 1);
    }
    return std::make_tuple(key.trimmed(), value.trimmed());
};

void configureUdpSocket(
    std::unique_ptr<nx::network::AbstractDatagramSocket>& socket,
    int recvBufferSize)
{
    socket->setRecvTimeout(500);
    if (recvBufferSize)
        socket->setRecvBufferSize(recvBufferSize);
    socket->setNonBlockingMode(true);
};

} // namespace

QString extractRtspParam(const QString& buffer, const QString& paramName);

const QByteArray QnRtspClient::kPlayCommand("PLAY");
const QByteArray QnRtspClient::kSetupCommand("SETUP");
const QByteArray QnRtspClient::kOptionsCommand("OPTIONS");
const QByteArray QnRtspClient::kDescribeCommand("DESCRIBE");
const QByteArray QnRtspClient::kSetParameterCommand("SET_PARAMETER");
const QByteArray QnRtspClient::kGetParameterCommand("GET_PARAMETER");
const QByteArray QnRtspClient::kPauseCommand("PAUSE");
const QByteArray QnRtspClient::kTeardownCommand("TEARDOWN");

//-------------------------------------------------------------------------------------------------
// QnRtspIoDevice

QnRtspIoDevice::QnRtspIoDevice(
    QnRtspClient* owner,
    nx::vms::api::RtpTransportType transport,
    quint16 mediaPort,
    quint16 rtcpPort)
    :
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
    if (m_transport == nx::vms::api::RtpTransportType::multicast)
    {
        if (m_udpSockets.mediaSocket)
            m_udpSockets.mediaSocket->leaveGroup(m_multicastAddress.toString().toStdString());
        if (m_udpSockets.rtcpSocket)
            m_udpSockets.rtcpSocket->leaveGroup(m_multicastAddress.toString().toStdString());
    }
}

void QnRtspIoDevice::bindToMulticastAddress(
    const QHostAddress& address,
    const nx::String& interfaceAddress)
{
    if (m_udpSockets.mediaSocket)
        m_udpSockets.mediaSocket->joinGroup(address, interfaceAddress.toStdString());
    if (m_udpSockets.rtcpSocket)
        m_udpSockets.rtcpSocket->joinGroup(address, interfaceAddress.toStdString());

    m_multicastAddress = address;
}

void QnRtspIoDevice::sendDummy()
{
    // If we're behind a NAT, send a few dummy UDP packets to the server.
    auto sendDummy = [this](nx::network::AbstractDatagramSocket* socket, int port)
    {
        if (!socket)
            return;

        uint32_t const dummy = 0xFEEDFACE;
        auto remoteEndpoint = nx::network::SocketAddress(m_hostAddress, port);
        if (!socket->setDestAddr(remoteEndpoint))
        {
            NX_WARNING(this, "Failed to set address for socket, host: %1, port: %2, error: %3",
                m_hostAddress, port, SystemError::getLastOSErrorText());
            return;
        }
        socket->send(&dummy, sizeof(dummy));
    };
    sendDummy(m_udpSockets.mediaSocket.get(), m_remoteMediaPort);
    sendDummy(m_udpSockets.rtcpSocket.get(), m_remoteRtcpPort);
}

qint64 QnRtspIoDevice::read(char *data, qint64 maxSize)
{
    int bytesRead;
    if (m_transport == nx::vms::api::RtpTransportType::tcp)
        bytesRead = m_owner->readBinaryResponse((quint8*) data, maxSize); // demux binary data from TCP socket
    else
        bytesRead = m_udpSockets.mediaSocket->recv(data, maxSize);

    m_owner->sendKeepAliveIfNeeded();
    if (m_transport == nx::vms::api::RtpTransportType::udp)
        processRtcpData();
    return bytesRead;
}

nx::network::AbstractCommunicatingSocket* QnRtspIoDevice::getMediaSocket()
{
    if (m_transport == nx::vms::api::RtpTransportType::tcp)
        return m_owner->m_tcpSock.get();
    else
        return m_udpSockets.mediaSocket.get();
}

void QnRtspIoDevice::shutdown()
{
    if (m_transport == nx::vms::api::RtpTransportType::tcp)
        m_owner->shutdown();
    else
        m_udpSockets.mediaSocket->shutdown();
}

bool QnRtspIoDevice::updateRemotePorts(quint16 mediaPort, quint16 rtcpPort)
{
    m_remoteMediaPort = mediaPort;
    m_remoteRtcpPort = rtcpPort;
    if (m_transport == nx::vms::api::RtpTransportType::multicast)
        return createMulticastSockets();
    return true;
}

bool QnRtspIoDevice::setTransport(nx::vms::api::RtpTransportType rtpTransport)
{
    if (m_transport == rtpTransport)
        return true;

    m_transport = rtpTransport;
    return updateSockets();
}

void QnRtspIoDevice::processRtcpData()
{
    quint8 rtcpBuffer[MAX_RTCP_PACKET_SIZE];
    quint8 sendBuffer[MAX_RTCP_PACKET_SIZE];

    bool rtcpReportAlreadySent = false;
    while( m_udpSockets.rtcpSocket->hasData() )
    {
        nx::network::SocketAddress senderEndpoint;
        int bytesRead = m_udpSockets.rtcpSocket->recvFrom(rtcpBuffer, sizeof(rtcpBuffer), &senderEndpoint);
        if (bytesRead > 0)
        {
            if (!m_udpSockets.rtcpSocket->isConnected())
            {
                if (!m_udpSockets.rtcpSocket->setDestAddr(senderEndpoint))
                {
                    qWarning() << "QnRtspIoDevice::processRtcpData(): setDestAddr() failed: " << SystemError::getLastOSErrorText().c_str();
                }
            }
            nx::streaming::rtp::RtcpSenderReport senderReport;
            if (senderReport.read(rtcpBuffer, bytesRead))
                m_senderReport = senderReport;
            int outBufSize = nx::streaming::rtp::buildClientRtcpReport(sendBuffer, MAX_RTCP_PACKET_SIZE);
            if (outBufSize > 0)
            {
                m_udpSockets.rtcpSocket->send(sendBuffer, outBufSize);
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
                if (!m_udpSockets.rtcpSocket->setDestAddr(remoteEndpoint))
                {
                    qWarning()
                        << "RTPIODevice::processRtcpData(): setDestAddr() failed: "
                        << SystemError::getLastOSErrorText().c_str();
                }
                m_udpSockets.rtcpSocket->send(sendBuffer, outBufSize);
            }

            m_reportTimer.restart();
        }
    }
}

std::unique_ptr<nx::network::AbstractDatagramSocket> QnRtspIoDevice::createMulticastSocket(int port)
{
    auto result = nx::network::SocketFactory::createDatagramSocket();
    result->setReuseAddrFlag(true);

    if (!result->bind(nx::network::SocketAddress(nx::network::HostAddress::anyHost, port)))
    {
        NX_WARNING(this, "Failed to bind multicast socket to port: %1, error: %2",
            port, SystemError::getLastOSErrorCode());
        return nullptr;
    }

    configureUdpSocket(result, m_owner->tcpRecvBufferSize());
    return result;
}

bool QnRtspIoDevice::createMulticastSockets()
{
    m_udpSockets.mediaSocket = createMulticastSocket(m_remoteMediaPort);
    m_udpSockets.rtcpSocket = createMulticastSocket(m_remoteRtcpPort);
    if (!m_udpSockets.mediaSocket || !m_udpSockets.rtcpSocket)
        return false;

    return true;
}

bool QnRtspIoDevice::updateSockets()
{
    if (m_transport == nx::vms::api::RtpTransportType::tcp)
    {
        m_udpSockets.mediaSocket.reset();
        m_udpSockets.rtcpSocket.reset();
        if (m_owner->m_tcpSock)
        {
            if (m_owner->tcpRecvBufferSize())
                m_owner->setTcpRecvBufferSize(m_owner->tcpRecvBufferSize());
        }
        return true;
    }

    if (m_transport == nx::vms::api::RtpTransportType::multicast)
        return createMulticastSockets();

    if (m_transport == nx::vms::api::RtpTransportType::udp)
    {
        if (!m_udpSockets.bind())
        {
            NX_WARNING(this, "Failed to bind UDP sockets");
            return false;
        }

        configureUdpSocket(m_udpSockets.mediaSocket, m_owner->tcpRecvBufferSize());
        configureUdpSocket(m_udpSockets.rtcpSocket, m_owner->tcpRecvBufferSize());
        return true;
    }
    return true;
}

QnRtspIoDevice::AddressInfo QnRtspIoDevice::mediaAddressInfo() const
{
    return addressInfo(m_remoteMediaPort);
}

QnRtspIoDevice::AddressInfo QnRtspIoDevice::rtcpAddressInfo() const
{
    return addressInfo(m_remoteRtcpPort);
}

QnRtspIoDevice::AddressInfo QnRtspIoDevice::addressInfo(int port) const
{
    AddressInfo info;
    info.transport = m_transport;
    info.address.address = m_transport == nx::vms::api::RtpTransportType::multicast
        ? nx::network::HostAddress(m_multicastAddress.toString().toStdString())
        : nx::network::HostAddress(m_hostAddress.toString());

    info.address.port = port;
    return info;
}

//-------------------------------------------------------------------------------------------------
// QnRtspClient

QnRtspClient::QnRtspClient(
    const Config& config)
    :
    m_config(config),
    m_csec(1),
    m_transport(nx::vms::api::RtpTransportType::udp),
    m_selectedAudioChannel(0),
    m_startTime(DATETIME_NOW),
    m_openedTime(AV_NOPTS_VALUE),
    m_endTime(AV_NOPTS_VALUE),
    m_scale(1.0),
    m_tcpTimeout(10 * 1000),
    m_userAgent(nx::network::http::userAgentString()),
    m_defaultAuthScheme(nx::network::http::header::AuthScheme::basic)
{
    m_responseBuffer = new quint8[RTSP_BUFFER_LEN];
    m_responseBufferLen = 0;
}

QnRtspClient::~QnRtspClient()
{
    stop();
    delete [] m_responseBuffer;
}

bool QnRtspClient::parseSDP(const QByteArray& response)
{
    int sdpIndex = response.indexOf(QLatin1String("\r\n\r\n"));
    if (sdpIndex < 0)
    {
        NX_WARNING(this, "Invalid DESCRIBE response, SDP delimiter not found: [%1]", response);
        return false;
    }

    m_sdp.parse(QString(response.mid(sdpIndex + 4)));

    // At this moment we do not support different transport for streams, so use first.
    if (m_sdp.media.size() > 0 && m_sdp.media[0].connectionAddress.isMulticast())
        m_actualTransport = m_transport = nx::vms::api::RtpTransportType::multicast;

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

    if (m_sdpTracks.size() <= 0)
    {
        NX_WARNING(this, "Invalid DESCRIBE response, no relevant media track found: [%1]",
            response);
        return false;
    }

    return true;
}

void QnRtspClient::parseRangeHeader(const QString& rangeStr)
{
    if (nx::network::rtsp::header::Range range; range.parse(rangeStr.toStdString()))
    {
        m_startTime = range.startUs.value_or(DATETIME_NOW);
        m_endTime = range.endUs.value_or(AV_NOPTS_VALUE);
    }
}

CameraDiagnostics::Result QnRtspClient::open(const nx::utils::Url& url, qint64 startTime)
{
    /*
     * Our own client uses CSEQ. This value is put to the media packets and player check whether it get old or new value
     * after the seek.
     */
    if (!m_additionAttrs.contains(Qn::EC2_INTERNAL_RTP_FORMAT))
        m_csec = 1;
    m_actualTransport = m_transport;
    if (m_actualTransport == nx::vms::api::RtpTransportType::automatic)
        m_actualTransport = nx::vms::api::RtpTransportType::tcp;

    if (startTime != AV_NOPTS_VALUE)
        m_openedTime = startTime;

    m_SessionId.clear();
    m_url = url;
    m_responseBufferLen = 0;
    m_isProxyAuthorizationRequired = false;
    if (m_defaultAuthScheme == nx::network::http::header::AuthScheme::basic)
        m_responseAuthenticate = nx::network::http::header::WWWAuthenticate(m_defaultAuthScheme);
    else
        m_responseAuthenticate.reset();

    bool isSslRequired = false;
    const auto urlScheme = m_url.scheme().toLower().toUtf8();
    if (urlScheme == nx::network::rtsp::kUrlSchemeName)
        isSslRequired = false;
    else if (urlScheme == nx::network::rtsp::kSecureUrlSchemeName)
        isSslRequired = true;
    else
        return CameraDiagnostics::UnsupportedProtocolResult(m_url, m_url.scheme());

    if (!NX_ASSERT(!m_credentials.authToken.isBearerToken() || isSslRequired, "Url: %1", m_url))
    {
        return CameraDiagnostics::RequestFailedResult(m_url.toString(),
            "Bearer token authorization can't be used with insecure url scheme.");
    }

    {
        NX_MUTEX_LOCKER lock(&m_socketMutex);
        m_tcpSock = nx::network::SocketFactory::createStreamSocket(
            nx::network::ssl::kAcceptAnyCertificate, isSslRequired);
    }

    m_tcpSock->setRecvTimeout(TCP_RECEIVE_TIMEOUT_MS);

    nx::network::SocketAddress targetAddress;
    if (m_proxyAddress)
        targetAddress = *m_proxyAddress;
    else
        targetAddress = nx::network::url::getEndpoint(m_url, DEFAULT_RTP_PORT);

    if (!m_tcpSock->connect(targetAddress, std::chrono::milliseconds(TCP_CONNECT_TIMEOUT_MS)))
        return CameraDiagnostics::CannotOpenCameraMediaPortResult(url, targetAddress.port);

    m_tcpSock->setNoDelay(true);
    m_tcpSock->setRecvTimeout(m_tcpTimeout);
    m_tcpSock->setSendTimeout(m_tcpTimeout);

    if (m_playNowMode)
    {
        m_contentBase = m_url.toString();
        return CameraDiagnostics::NoErrorResult();
    }

    auto result = sendOptions();
    if (!result)
    {
        stop();
        return result;
    }

    result = sendDescribe();
    if (result)
        NX_DEBUG(this, "Successfully opened RTSP stream %1", m_url);

    return result;
}

CameraDiagnostics::Result QnRtspClient::sendDescribe()
{
    QByteArray response;
    auto result = sendRequestAndReceiveResponse(createDescribeRequest(), response);
    if (!result)
    {
        stop();
        return result;
    }

    QString rangeStr = extractRtspParam(QLatin1String(response), QLatin1String("Range:"));
    if (!rangeStr.isEmpty())
        parseRangeHeader(rangeStr);

    if (!parseSDP(response))
    {
        stop();
        return CameraDiagnostics::NoMediaTrackResult(m_url);
    }

    m_contentBase = parseContentBase(response);
    return CameraDiagnostics::NoErrorResult();
}

QString QnRtspClient::parseContentBase(const QString& response)
{
    /*
     * RFC2326
     * 1. The RTSP Content-Base field.
     * 2. The RTSP Content-Location field.
     * 3. The RTSP request URL.
     * If this attribute contains only an asterisk (*), then the URL is
     * treated as if it were an empty embedded URL, and thus inherits the
     * entire base URL.
    */
    QString contentBase = extractRtspParam(response, "Content-Base:");
    if (!contentBase.isEmpty())
    {
        if (nx::utils::Url(contentBase).isValid())
            return contentBase;
        NX_DEBUG(this, "Invalid Content-Base url: [%1], ignore it", contentBase);
    }

    contentBase = extractRtspParam(response, "Content-Location:");
    if (!contentBase.isEmpty())
    {
        if (nx::utils::Url(contentBase).isValid())
            return contentBase;
        NX_DEBUG(this, "Invalid Content-Location url: [%1], ignore it", contentBase);
    }

    return m_url.toString(); // TODO remove url params?
}

bool QnRtspClient::play(qint64 positionStart, qint64 positionEnd, double scale)
{
    if (!m_playNowMode && !sendSetup())
    {
        NX_WARNING(this, "Failed to send SETUP command");
        m_sdpTracks.clear();
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
    NX_MUTEX_LOCKER lock(&m_socketMutex);
    m_tcpSock.reset();
    return true;
}

void QnRtspClient::shutdown()
{
    NX_MUTEX_LOCKER lock(&m_socketMutex);
    if (m_tcpSock)
        m_tcpSock->shutdown();
}

bool QnRtspClient::isOpened() const
{
    NX_MUTEX_LOCKER lock(&m_socketMutex);
    return m_tcpSock && m_tcpSock->isConnected();
}

unsigned int QnRtspClient::sessionTimeoutMs()
{
    return duration_cast<milliseconds>(m_keepAliveTimeOut).count();
}

const nx::streaming::Sdp& QnRtspClient::getSdp() const
{
    return m_sdp;
}

void QnRtspClient::setPreferredMap(const nx::streaming::Sdp::RtpMap& map)
{
    m_sdp.preferredMap = map;
}

void QnRtspClient::fillRequestAuthorizationByResponseAuthenticate(
    nx::network::http::Request* request)
{
    using namespace nx::network::http;
    const char* headerName =
        m_isProxyAuthorizationRequired ? header::kProxyAuthorization : header::Authorization::NAME;
    if (m_responseAuthenticate->authScheme == header::AuthScheme::digest)
    {
        header::DigestAuthorization authorization;
        if (calcDigestResponse(
            request->requestLine.method,
            m_credentials,
            m_url.toStdString(),
            *m_responseAuthenticate,
            &authorization,
            ++m_nonceCount))
        {
            insertOrReplaceHeader(
                &request->headers, HttpHeader(headerName, authorization.serialized()));
        }
        return;
    }
    if (m_responseAuthenticate->authScheme == header::AuthScheme::basic)
    {
        const auto& token = m_credentials.authToken;
        if (token.isPassword() || token.empty())
        {
            insertOrReplaceHeader(&request->headers,
                HttpHeader(headerName,
                    header::BasicAuthorization(m_credentials.username, token.value).serialized()));
        }
    }
}

void QnRtspClient::fillRequestAuthorization(nx::network::http::Request* request)
{
    using namespace nx::network::http;
    const auto& token = m_credentials.authToken;

    // Avoid setting auth headers when credentials are empty.
    if (token.empty() && m_credentials.username.empty())
        return;

    if (token.isBearerToken())
    {
        insertOrReplaceHeader(&request->headers,
            HttpHeader(header::Authorization::NAME,
                header::BearerAuthorization(token.value).serialized()));
        return;
    }
    if (m_responseAuthenticate)
    {
        fillRequestAuthorizationByResponseAuthenticate(request);
        return;
    }
    if (m_config.shouldGuessAuthDigest && (token.isPassword() || token.empty()))
    {
        // Trying to "guess" authentication header.
        // It is used in the client/server conversations,
        // since client can effectively predict nonce and realm.
        QAuthenticator auth;
        auth.setUser(nx::toString(m_credentials.username));
        auth.setPassword(nx::toString(token.value));

        using namespace nx::network::http;

        header::DigestAuthorization authorization;
        header::WWWAuthenticate authHeader(header::AuthScheme::digest);
        authHeader.params["realm"] = nx::network::AppInfo::realm();
        authHeader.params["nonce"] = TimeBasedNonceProvider::generateTimeBasedNonce();

        const auto calcRes = calcDigestResponse(
            request->requestLine.method,
            auth,
            request->requestLine.url.toStdString(),
            authHeader,
            &authorization);
        if (calcRes)
        {
            insertOrReplaceHeader(&request->headers,
                HttpHeader(header::Authorization::NAME, authorization.serialized()));
        }
    }
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
            nx::network::url::getEndpoint(m_url).toString()));
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
    if (!m_tcpSock)
        return false;

    fillRequestAuthorization(&request);
    addAdditionAttrs(&request);
    nx::Buffer requestBuf;
    request.serialize( &requestBuf );
    return m_tcpSock->send(requestBuf.data(), requestBuf.size()) > 0;
}

CameraDiagnostics::Result QnRtspClient::sendOptions()
{
    nx::network::http::Request request;
    request.requestLine.method = kOptionsCommand;
    request.requestLine.url = m_url;
    request.requestLine.version = nx::network::rtsp::rtsp_1_0;
    addCommonHeaders(request.headers);
    QByteArray response;
    const auto result = sendRequestAndReceiveResponse(std::move(request), response);
    if (!result)
        NX_DEBUG(this, "OPTIONS request failed for URL %1", request.requestLine.url);

    QString allowedMethods = extractRtspParam(QLatin1String(response), QLatin1String("Public:"));
    m_keepAliveSupported = allowedMethods.contains("GET_PARAMETER");
    if (!m_keepAliveSupported)
    {
        NX_DEBUG(this, "GET_PARAMETER method not allowed, disable keep alive, public methods: %1",
            allowedMethods);
    }

    return result;
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
    if (!m_tcpSock)
        return false;
    const nx::String localAddress = m_tcpSock->getLocalAddress().address.toString();

    using namespace nx::streaming;

    if (m_sdpTracks.empty())
    {
        NX_WARNING(this, "Failed to start rtsp session, empty SDP");
        return false;
    }
    m_keepAliveTimeOut = std::chrono::seconds(60);
    int audioNum = 0;
    for (uint32_t i = 0; i < m_sdpTracks.size(); ++i)
    {
        auto& track = m_sdpTracks[i];
        const QString codecName = track.sdpMedia.rtpmap.codecName;

        const bool isAdditionalSupportedCodec = codecName == METADATA_STR
            || m_additionalSupportedCodecs.find(codecName) != m_additionalSupportedCodecs.cend();

        if (!isMediaTypeEnabled(track.sdpMedia.mediaType))
            continue;

        if (track.sdpMedia.mediaType == Sdp::MediaType::Audio)
        {
            if (audioNum++ != m_selectedAudioChannel)
                continue;
        }
        else if (track.sdpMedia.mediaType != Sdp::MediaType::Video && !isAdditionalSupportedCodec)
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
            // SETUP postfix should be written after url query params. It's invalid url, but it's required according to RTSP standard
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
            nx::String transportStr = "RTP/AVP/";
            transportStr += m_actualTransport == nx::vms::api::RtpTransportType::tcp
                ? "TCP"
                : "UDP";

            transportStr += m_actualTransport == nx::vms::api::RtpTransportType::multicast
                ? ";multicast;"
                : ";unicast;";

            if (!track.ioDevice->setTransport(m_actualTransport))
                return false;

            if (m_actualTransport != nx::vms::api::RtpTransportType::tcp)
            {
                transportStr += m_actualTransport == nx::vms::api::RtpTransportType::multicast
                    ? "port="
                    : "client_port=";

                transportStr += track.ioDevice->getUdpSockets().getPortsString();
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

        QByteArray response;
        if (!sendRequestAndReceiveResponse(std::move(request), response))
        {
            if (m_transport == nx::vms::api::RtpTransportType::automatic
                && m_actualTransport == nx::vms::api::RtpTransportType::tcp)
            {
                m_actualTransport = nx::vms::api::RtpTransportType::udp;
                if (!sendSetup()) //< Try UDP transport.
                    return false;
            }
            else
                return false;
        }

        track.setupSuccess = true;
        if (!parseSetupResponse(response, &track, i))
            return false;

        if (m_transport == nx::vms::api::RtpTransportType::multicast)
            track.ioDevice->bindToMulticastAddress(track.sdpMedia.connectionAddress, localAddress);

        updateTransportHeader(response);

        if (m_actualTransport == nx::vms::api::RtpTransportType::udp)
            track.ioDevice->sendDummy(); //< NAT traversal.
    }
    return true;
}

bool QnRtspClient::parseSetupResponse(const QString& response, SDPTrackInfo* track, int trackIndex)
{
    QString sessionParam = extractRtspParam(response, QLatin1String("Session:"));
    bool isFirstParam = true;
    for (const auto& parameter: sessionParam.split(';', Qt::SkipEmptyParts))
    {
        const auto [key, value] = splitKeyValue(parameter.trimmed(), '=');
        if (key.toLower() == "timeout")
        {
            const auto timeoutSec = value.toInt();
            if (timeoutSec > 0 && timeoutSec < 5000)
            {
                m_keepAliveTimeOut = seconds(timeoutSec);
                NX_DEBUG(this,
                    "Session timeout specified: %1", duration_cast<seconds>(m_keepAliveTimeOut));
            }
            else
            {
                NX_DEBUG(this,
                    "Invalid session timeout specified: [%1], will used %2 seconds",
                    value, duration_cast<seconds>(m_keepAliveTimeOut));
            }
        }
        else if (isFirstParam)
        {
            m_SessionId = key;
            isFirstParam = false;
        }
    }

    QString transportParam = extractRtspParam(response, "Transport:");

    for (const auto& parameter: transportParam.split(';', Qt::SkipEmptyParts))
    {
        const auto [key, value] = splitKeyValue(parameter.trimmed().toLower(), '=');
        if (key.isEmpty() || value.isEmpty())
            continue;
        auto [part1, part2] = splitKeyValue(value, '-'); //< Used for range parameters.

        if (key == "ssrc")
        {
            bool ok;
            track->ioDevice->setSSRC((quint32)value.toLongLong(&ok, 16));
        }
        else if (key == "interleaved")
        {
            track->interleaved = {part1.toInt(),
                part2.isEmpty() ? part1.toInt() + 1 : part2.toInt()};
            registerRTPChannel(track->interleaved.first, track->interleaved.second, trackIndex);
        }
        else if (key == "server_port" || key == "port")
        {
            if (!track->ioDevice->updateRemotePorts(part1.toUShort(),
                part2.isEmpty() ? part1.toUShort() + 1 : part2.toUShort()))
            {
                return false;
            }
        }
        else if (key == "destination")
        {
            track->sdpMedia.connectionAddress = QHostAddress(value);
        }
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

bool QnRtspClient::sendSetParameter(const QByteArray& paramName, const QByteArray& paramValue)
{
    NX_VERBOSE(this, "Sending SetParameter %1: %2", paramName, paramValue);
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
    request.headers.insert( nx::network::http::HttpHeader( "Content-Length", nx::utils::to_string(request.messageBody.size()) ) );
    return sendRequestInternal(std::move(request));
}

void QnRtspClient::addRangeHeader( nx::network::http::Request* const request, qint64 startPos, qint64 endPos )
{
    if (startPos != qint64(AV_NOPTS_VALUE))
    {
        nx::network::rtsp::header::Range range;

        //There is no guarantee that every RTSP server understands utc ranges.
        //RFC requires only npt ranges support.
        range.type =
            [&]()
            {
                switch (m_dateTimeFormat)
                {
                    case DateTimeFormat::Numeric:
                        if (!m_additionAttrs.contains(Qn::EC2_INTERNAL_RTP_FORMAT)
                            && startPos == DATETIME_NOW)
                        {
                            // Use rfc format for cameras if it not configured.
                            return nx::network::rtsp::header::Range::Type::npt;
                        }
                        return nx::network::rtsp::header::Range::Type::nxClock;
                    case DateTimeFormat::ISO:
                        return nx::network::rtsp::header::Range::Type::clock;
                }

                NX_ASSERT(false);
                return nx::network::rtsp::header::Range::Type{};
            }();

        range.startUs = startPos;

        if (endPos != qint64(AV_NOPTS_VALUE))
            range.endUs = endPos;

        nx::network::http::insertOrReplaceHeader(
            &request->headers,
            nx::network::http::HttpHeader("Range", range.serialize()));
    }
}

QByteArray QnRtspClient::getGuid()
{
    // client guid. used in proprietary extension.
    static QByteArray s_guid(QnUuid::createUuid().toString().toUtf8());
    return s_guid;
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
        QString cseq = extractRtspParam(QLatin1String(response), QLatin1String("CSeq:"));
        if (cseq.toInt() == (int)m_csec-1)
            break;
        else if (!readTextResponse(response))
            return false; // try next response
    }

    QString tmp = extractRtspParam(QLatin1String(response), QLatin1String("Range:"));
    if (!tmp.isEmpty())
        parseRangeHeader(tmp);

    tmp = extractRtspParam(QLatin1String(response), QLatin1String("x-video-layout:"));
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

void QnRtspClient::sendKeepAliveIfNeeded()
{
    if (m_config.disableKeepAlive || !m_keepAliveSupported)
        return;

    if (milliseconds(m_keepAliveTime.elapsed()) < m_keepAliveTimeOut * kKeepAliveGuardInterval)
        return;

    if (!sendKeepAlive())
        NX_WARNING(this, "Failed to send keep alive message");

    m_keepAliveTime.restart();
}

bool QnRtspClient::sendKeepAlive()
{
    NX_DEBUG(this, "Send keep alive message, keepAliveTimeOut: %1, url: %2",
        m_keepAliveTimeOut, m_url);
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
    if (m_tcpSock)
        m_tcpSock->send(buffer, size);
}

void QnRtspClient::processTextData(const QByteArray& textData)
{
    const int requestLineLen = textData.indexOf("\r\n");
    if (requestLineLen == -1)
        return;
    nx::network::http::RequestLine requestLine;
    requestLine.parse(std::string_view(textData.data(), requestLineLen));

    if (requestLine.method == "SET_PARAMETER")
    {
        nx::network::http::Response response;
        response.statusLine.version = nx::network::rtsp::rtsp_1_0;
        response.statusLine.statusCode = 200;
        response.statusLine.reasonPhrase = "OK";

        response.headers.insert({"User-Agent", m_userAgent});

        nx::network::http::Request request;
        request.parse(std::string_view(textData.data(), textData.size()));
        for (const auto& header: request.headers)
        {
            const QString lowerName = QString::fromStdString(header.first).toLower();
            if (lowerName == "cseq" || lowerName == "session")
                response.headers.insert(header);
        }

        nx::Buffer buffer;
        response.serialize(&buffer);
        if (m_tcpSock->send(buffer.data(), buffer.size()) <= 0)
            NX_DEBUG(this, "Failed to send response: %1", SystemError::getLastOSErrorText());

        return;
    }

    const QString range = extractRtspParam(textData, "Range:");
    if (!range.isEmpty())
        parseRangeHeader(range);
}

bool QnRtspClient::readAndProcessTextData()
{
    // have text response or part of text response.
    if (!m_tcpSock)
        return false;
    int bytesRead = readSocketWithBuffering(m_responseBuffer+m_responseBufferLen, qMin(1024, RTSP_BUFFER_LEN - m_responseBufferLen), true);
    if (bytesRead <= 0)
        return false;
    m_responseBufferLen += bytesRead;

    const quint8* textBlockEnd = m_responseBuffer;
    const quint8* bEnd = m_responseBuffer + m_responseBufferLen;
    for(; textBlockEnd < bEnd && *textBlockEnd != '$'; textBlockEnd++);

    while (textBlockEnd > m_responseBuffer)
    {
        const auto messageSize = QnTCPConnectionProcessor::isFullMessage(QByteArray::fromRawData(
            (const char*) m_responseBuffer, textBlockEnd - m_responseBuffer));
        if (messageSize <= 0)
            break;

        QByteArray textData = QByteArray::fromRawData((const char*) m_responseBuffer, messageSize);
        processTextData(textData);
        const quint8* messageEnd = m_responseBuffer + messageSize;
        memmove(m_responseBuffer, messageEnd, bEnd - messageEnd);
        m_responseBufferLen -= messageSize;
        textBlockEnd -= messageSize;
    };

    return true;
}

int QnRtspClient::readBinaryResponse(quint8* data, int maxDataSize)
{
    if (!m_tcpSock)
        return 0;
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
        if (!readAndProcessTextData())
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
        demuxedData.resize(channel+1, nullptr);
    if (demuxedData[channel] == 0)
        demuxedData[channel] = new QnByteArray(16, 32, /*AV_INPUT_BUFFER_PADDING_SIZE*/ 32);
    QnByteArray* dataVect = demuxedData[channel];
    //dataVect->resize(dataVect->size() + reserve);
    dataVect->reserve(dataVect->size() + reserve);
    return (quint8*) dataVect->data() + dataVect->size();
}

int QnRtspClient::readBinaryResponse(std::vector<QnByteArray*>& demuxedData, int& channelNumber)
{
    if (!m_tcpSock)
        return 0;
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
        if (!readAndProcessTextData())
            return -1;
    }

    int dataLen = (m_responseBuffer[2]<<8) + m_responseBuffer[3] + 4;
    int copyLen = qMin(dataLen, m_responseBufferLen);
    channelNumber = m_responseBuffer[1];
    quint8* data = prepareDemuxedData(demuxedData, channelNumber, dataLen);

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
bool QnRtspClient::readTextResponse(QByteArray& response)
{
    if (!m_tcpSock)
        return false;
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
                    NX_DEBUG(this, "RTSP connection to %1 has been unexpectedly closed",
                        m_tcpSock->getForeignAddress());
                }
                else if (!m_tcpSock->isClosed())
                {
                    NX_DEBUG(this, "Error reading RTSP response from %1. %2",
                        m_tcpSock->getForeignAddress(), SystemError::getLastOSErrorText());
                }
                return false; //< error occurred or connection closed
            }
            m_responseBufferLen += bytesRead;
        }
        if (m_responseBuffer[0] == '$') {
            // binary data
            quint8 tmpData[RTSP_BUFFER_LEN];
            int bytesRead = readBinaryResponse(tmpData, sizeof(tmpData)); // skip binary data
            if (bytesRead < 0)
            {
                NX_DEBUG(this, "Failed to read data from socket: %1",
                    m_tcpSock->getForeignAddress());
                return false;
            }

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
            NX_DEBUG(this, "RTSP response from %1 has exceeded max response size (%2)",
                m_tcpSock->getForeignAddress(), RTSP_BUFFER_LEN);
            return false;
        }
    }
    return false;
}

QString extractRtspParam(const QString& buffer, const QString& paramName)
{
    int begin = buffer.indexOf(paramName, 0, Qt::CaseInsensitive);
    if (begin == -1)
        return "";

    begin += paramName.size();

    const int end = buffer.indexOf("\r\n", begin);
    if (end == -1)
        return "";

    return buffer.mid(begin, end - begin).trimmed();
}

void QnRtspClient::updateTransportHeader(QByteArray& response)
{
    QString tmp = extractRtspParam(QLatin1String(response), QLatin1String("Transport:"));
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

void QnRtspClient::setTransport(nx::vms::api::RtpTransportType transport)
{
    m_transport = transport;
}

void QnRtspClient::setAdditionalSupportedCodecs(std::set<QString> additionalSupportedCodecs)
{
    m_additionalSupportedCodecs = std::move(additionalSupportedCodecs);
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
    NX_MUTEX_LOCKER lock(&m_socketMutex);
    if (m_tcpSock)
    {
        m_tcpSock->setRecvTimeout(m_tcpTimeout);
        m_tcpSock->setSendTimeout(m_tcpTimeout);
    }
}

void QnRtspClient::setTcpRecvBufferSize(int value)
{
    m_tcpRecvBufferSize = value;
    NX_MUTEX_LOCKER lock(&m_socketMutex);
    if (m_tcpRecvBufferSize && m_tcpSock)
        m_tcpSock->setRecvBufferSize(m_tcpRecvBufferSize);
}

std::chrono::milliseconds QnRtspClient::getTCPTimeout() const
{
    return m_tcpTimeout;
}

void QnRtspClient::setUsernameAndPassword(
    const QString& username,
    const QString& password,
    nx::network::http::header::AuthScheme::Value defaultAuthScheme)
{
    setCredentials(
        nx::network::http::Credentials(
            username.toStdString(),
            nx::network::http::PasswordAuthToken(password.toStdString())),
        defaultAuthScheme);
}

void QnRtspClient::setCredentials(const nx::network::http::Credentials& credentials,
    nx::network::http::header::AuthScheme::Value defaultAuthScheme)
{
    using namespace nx::network::http;
    m_credentials = credentials;
    switch (credentials.authToken.type)
    {
        case nx::network::http::AuthTokenType::none:
            m_defaultAuthScheme = nx::network::http::header::AuthScheme::none;
            return;
        case nx::network::http::AuthTokenType::password:
            NX_ASSERT(defaultAuthScheme == header::AuthScheme::basic
                || defaultAuthScheme == header::AuthScheme::digest);
            m_defaultAuthScheme = defaultAuthScheme;
            return;
        case nx::network::http::AuthTokenType::ha1:
            m_defaultAuthScheme = nx::network::http::header::AuthScheme::digest;
            return;
        case nx::network::http::AuthTokenType::bearer:
            m_defaultAuthScheme = nx::network::http::header::AuthScheme::bearer;
            return;
    }
    NX_ASSERT(false, NX_FMT("Unknown AuthTokenType %1", (int) credentials.authToken.type));
}

const nx::network::http::Credentials& QnRtspClient::getCredentials() const
{
    return m_credentials;
}

nx::utils::Url QnRtspClient::getUrl() const
{
    return m_url;
}

void QnRtspClient::setMediaTypeEnabled(nx::streaming::Sdp::MediaType mediaType, bool enabled)
{
    if (enabled)
        m_disabledMediaTypes.erase(mediaType);
    else
        m_disabledMediaTypes.insert(mediaType);
}

bool QnRtspClient::isMediaTypeEnabled(nx::streaming::Sdp::MediaType mediaType) const
{
    return !m_disabledMediaTypes.contains(mediaType);
}

void QnRtspClient::setProxyAddr(const nx::String& addr, int port)
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

QString QnRtspClient::getVideoLayout() const
{
    return m_videoLayout;
}

#if defined(Q_OS_LINUX)
void sleepIfEmptySocket(nx::network::AbstractStreamSocket* socket)
{
    static const size_t ADDITIONAL_READ_BUFFER_CAPACITY = 64 * 1024;
    static const size_t MS_PER_SEC = 1000;
    static const size_t MAX_BITRATE_BITS_PER_SECOND = 50*1024*1024;
    static const size_t MAX_BITRATE_BYTES_PER_SECOND = MAX_BITRATE_BITS_PER_SECOND / CHAR_BIT;
    static_assert(
        MAX_BITRATE_BYTES_PER_SECOND > ADDITIONAL_READ_BUFFER_CAPACITY * 10,
        "MAX_BITRATE_BYTES_PER_SECOND MUST be 10 times greater than ADDITIONAL_READ_BUFFER_CAPACITY");

    //TODO #akolesnikov Better to find other solution and remove this code.
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
#endif // defined(Q_OS_LINUX)

int QnRtspClient::readSocketWithBuffering(quint8* buf, size_t bufSize, bool readSome)
{
    #if defined(Q_OS_LINUX)
        if (m_config.sleepIfEmptySocket)
            sleepIfEmptySocket(m_tcpSock.get());
    #endif

    int bytesRead = m_tcpSock->recv(buf, (unsigned int) bufSize, readSome ? 0 : MSG_WAITALL);
    if (bytesRead > 0)
        m_lastReceivedDataTimer.restart();
    return bytesRead;
}

CameraDiagnostics::Result QnRtspClient::sendRequestAndReceiveResponse(
    nx::network::http::Request&& request, QByteArray& responseBuf)
{
    nx::network::rtsp::StatusCodeValue prevStatusCode = nx::network::http::StatusCode::ok;

    if (request.requestLine.method != kOptionsCommand)
        addAdditionAttrs(&request);

    NX_VERBOSE(this, "Send: %1", request.requestLine.toString());
    const int port = m_url.port(DEFAULT_RTP_PORT);

    if (!m_tcpSock)
        return CameraDiagnostics::ConnectionClosedUnexpectedlyResult(m_url.host(), port);

    for (int i = 0; i < 3; ++i) //< Avoid infinite loop in case of incorrect server behavior.
    {
        fillRequestAuthorization(&request);

        nx::Buffer requestBuf;
        request.serialize( &requestBuf );
        if( m_tcpSock->send(requestBuf.data(), requestBuf.size()) <= 0 )
        {
            NX_DEBUG(this, "Failed to send request: %1", SystemError::getLastOSErrorText());
            return CameraDiagnostics::ConnectionClosedUnexpectedlyResult(m_url.host(), port);
        }

        if( !readTextResponse(responseBuf) )
        {
            NX_DEBUG(this, "Failed to read response");
            return CameraDiagnostics::ConnectionClosedUnexpectedlyResult(m_url.host(), port);
        }

        // Workaround for some buggy cameras that send '\r\n' after SDP in addition to content-length.
        if (responseBuf.startsWith("\r\n") && request.requestLine.method == kSetupCommand)
            responseBuf.remove(0, 2);

        nx::network::rtsp::RtspResponse response;
        if (!response.parse(nx::ConstBufferRefType(responseBuf.data(), responseBuf.size())))
        {
            NX_DEBUG(this, "Failed to parse response: '%1', method %2",
                responseBuf, request.requestLine.method);
            return CameraDiagnostics::ConnectionClosedUnexpectedlyResult(m_url.host(), port);
        }

        m_serverInfo = nx::network::http::getHeaderValue(
            response.headers, nx::network::http::header::Server::NAME);

        switch (response.statusLine.statusCode)
        {
            case nx::network::http::StatusCode::ok:
                return CameraDiagnostics::NoErrorResult();

            case nx::network::http::StatusCode::unauthorized:
            case nx::network::http::StatusCode::proxyAuthenticationRequired:
                if (m_credentials.authToken.isBearerToken())
                {
                    NX_DEBUG(this, "Bearer authentication failed");
                    return CameraDiagnostics::NotAuthorisedResult(m_url);
                }

                if (prevStatusCode == response.statusLine.statusCode)
                {
                    NX_DEBUG(this, "Already tried authentication and have been rejected");
                    return CameraDiagnostics::NotAuthorisedResult(m_url);
                }

                prevStatusCode = response.statusLine.statusCode;
                break;

            default:
                NX_DEBUG(this, "Response failed: %1", response.statusLine.toString());
                return CameraDiagnostics::RequestFailedResult(
                    m_url.toString(), nx::String(response.statusLine.reasonPhrase));
        }

        addCommonHeaders(request.headers); //< Update sequence after unauthorized response.

        m_isProxyAuthorizationRequired =
            response.statusLine.statusCode == nx::network::http::StatusCode::proxyAuthenticationRequired;
        const auto authenticateHeaderName =
            m_isProxyAuthorizationRequired ? "Proxy-Authenticate" : "WWW-Authenticate";
        const auto& authenticateHeaderValue =
            nx::network::http::getHeaderValue(response.headers, authenticateHeaderName);
        if (!authenticateHeaderValue.empty())
        {
            nx::network::http::header::WWWAuthenticate authenticate;
            if (!authenticate.parse(authenticateHeaderValue))
            {
                NX_DEBUG(this,
                    "Authentication to %1 failed with wrong %2 header value \"%3\" in response.",
                    m_url, authenticateHeaderName, authenticateHeaderValue);
                return CameraDiagnostics::NotAuthorisedResult(m_url);
            }
            if (m_ignoreQopInDigest)
                authenticate.params.erase("qop");
            m_responseAuthenticate = std::move(authenticate);
        }
    }

    NX_VERBOSE(this, "Response after last retry: %1", prevStatusCode);
    return CameraDiagnostics::ConnectionClosedUnexpectedlyResult(m_url.host(), port);
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
    NX_MUTEX_LOCKER lock(&m_socketMutex);
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

void QnRtspClient::setIgnoreQopInDigest(bool value)
{
    m_ignoreQopInDigest = value;
}

bool QnRtspClient::ignoreQopInDigest(bool value) const
{
    return m_ignoreQopInDigest;
}
