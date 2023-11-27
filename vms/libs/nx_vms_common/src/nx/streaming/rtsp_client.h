// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <vector>

#include <QtNetwork/QAuthenticator>

extern "C" {
// For const AV_NOPTS_VALUE.
#include <libavutil/avutil.h>
}

#include <QtCore/QDateTime>
#include <QtCore/QElapsedTimer>
#include <QtCore/QUrl>

#include <nx/network/http/auth_tools.h>
#include <nx/network/rtsp/rtsp_types.h>
#include <nx/network/socket.h>
#include <nx/rtp/rtcp.h>
#include <nx/rtp/sdp.h>
#include <nx/streaming/udp_socket_pair.h>
#include <nx/string.h>
#include <nx/utils/byte_array.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/vms/api/types/rtp_types.h>
#include <utils/camera/camera_diagnostics.h>
#include <utils/common/threadqueue.h>

class QnRtspClient;

static const int MAX_RTCP_PACKET_SIZE = 1024 * 2;
static const int MAX_RTP_PACKET_SIZE = 1024 * 16;

class NX_VMS_COMMON_API QnRtspIoDevice
{
public:
    struct AddressInfo
    {
        nx::vms::api::RtpTransportType transport{ nx::vms::api::RtpTransportType::automatic };
        nx::network::SocketAddress address;
    };

public:
    explicit QnRtspIoDevice(
        QnRtspClient* owner,
        nx::vms::api::RtpTransportType rtpTransport,
        quint16 mediaPort = 0,
        quint16 rtcpPort = 0);

    virtual ~QnRtspIoDevice();
    virtual qint64 read(char * data, qint64 maxSize );
    const nx::rtp::RtcpSenderReport& getSenderReport() { return m_senderReport; }
    void setSenderReport(const nx::rtp::RtcpSenderReport& value) { m_senderReport = value; }
    nx::network::AbstractCommunicatingSocket* getMediaSocket();
    nx::streaming::rtsp::UdpSocketPair& getUdpSockets() { return m_udpSockets; }
    void shutdown();
    bool setTransport(nx::vms::api::RtpTransportType rtpTransport);
    void setSSRC(quint32 value) {ssrc = value; }
    quint32 getSSRC() const { return ssrc; }

    bool updateRemotePorts(quint16 mediaPort, quint16 rtcpPort);
    void setHostAddress(const nx::network::HostAddress& hostAddress) {m_hostAddress = hostAddress;};
    void setForceRtcpReports(bool force) {m_forceRtcpReports = force;};

    void bindToMulticastAddress(const QHostAddress& address, const nx::String& interfaceAddress);
    void sendDummy();

    AddressInfo mediaAddressInfo() const;
    AddressInfo rtcpAddressInfo() const;

private:
    AddressInfo addressInfo(int port) const;
    void processRtcpData();
    bool updateSockets();
    std::unique_ptr<nx::network::AbstractDatagramSocket> createMulticastSocket(int port);
    bool createMulticastSockets();

private:
    QnRtspClient* m_owner = nullptr;
    nx::vms::api::RtpTransportType m_transport = nx::vms::api::RtpTransportType::automatic;

    nx::streaming::rtsp::UdpSocketPair m_udpSockets;
    nx::rtp::RtcpSenderReport m_senderReport;

    quint16 m_remoteMediaPort = 0;
    quint16 m_remoteRtcpPort = 0;
    nx::network::HostAddress m_hostAddress;
    quint32 ssrc = 0;
    QElapsedTimer m_reportTimer;
    bool m_reportTimerStarted = false;
    bool m_forceRtcpReports = false;
    QHostAddress m_multicastAddress;
};

inline bool operator<(
    const QnRtspIoDevice::AddressInfo& lhs,
    const QnRtspIoDevice::AddressInfo& rhs)
{
    if (lhs.transport != rhs.transport)
        return lhs.transport < rhs.transport;

    return lhs.address < rhs.address;
}

class NX_VMS_COMMON_API QnRtspClient
{
public:
    struct Config
    {
        bool shouldGuessAuthDigest = false;
        bool backChannelAudioOnly = false;
        bool disableKeepAlive = false;
        bool sleepIfEmptySocket = false;
    };

    static const QByteArray kPlayCommand;
    static const QByteArray kSetupCommand;
    static const QByteArray kOptionsCommand;
    static const QByteArray kDescribeCommand;
    static const QByteArray kSetParameterCommand;
    static const QByteArray kGetParameterCommand;
    static const QByteArray kPauseCommand;
    static const QByteArray kTeardownCommand;

    enum class DateTimeFormat
    {
        Numeric,
        ISO
    };

    struct SDPTrackInfo
    {
        SDPTrackInfo() = default;
        SDPTrackInfo(
            const nx::rtp::Sdp::Media& sdpMedia,
            QnRtspClient* owner,
            nx::vms::api::RtpTransportType transport,
            int serverPort)
            :
            sdpMedia(sdpMedia)
        {
            ioDevice = std::make_shared<QnRtspIoDevice>(owner, transport, serverPort);
            ioDevice->setHostAddress(nx::network::HostAddress(owner->getUrl().host().toStdString()));
        }
        bool setupSuccess = false;
        nx::rtp::Sdp::Media sdpMedia;
        QPair<int, int> interleaved{ -1, -1 };
        std::shared_ptr<QnRtspIoDevice> ioDevice;
    };
    QnRtspClient(const Config& config);

    ~QnRtspClient();

    /**
     * Shutdown TCP socket and terminate current IO operation. Can be called from the other thread.
     * Thread safe.
     */
    void shutdown();

    /**
     * Thread safe.
     * @return true if RTSP connection is opened.
     */
    bool isOpened() const;

    /**
     * Set TCP timeout on RTSP connection.
     * Thread safe.
     */
    void setTCPTimeout(std::chrono::milliseconds timeout);

    /**
     * Set socket buffer size on RTSP connection for media data. If zero, than use system default.
     * Thread safe.
     */
    void setTcpRecvBufferSize(int value);
    int tcpRecvBufferSize() const { return m_tcpRecvBufferSize; }

    // returns \a CameraDiagnostics::ErrorCode::noError if stream was opened, error code - otherwise
    CameraDiagnostics::Result open(const nx::utils::Url& url, qint64 startTime = AV_NOPTS_VALUE);

    /*
    * Start playing RTSP session.
    * @param positionStart start position at mksec
    * @param positionEnd end position at mksec
    * @param scale playback speed
    */
    bool play(qint64 positionStart, qint64 positionEnd, double scale);

    // returns true if there is no error delivering STOP
    bool stop();

    // session timeout in ms
    unsigned int sessionTimeoutMs();

    const nx::rtp::Sdp& getSdp() const;

    void sendKeepAliveIfNeeded();

    void setTransport(nx::vms::api::RtpTransportType transport);

    void setAdditionalSupportedCodecs(std::set<QString> additionalSupportedCodecs);

    // RTP transport configured by user
    nx::vms::api::RtpTransportType getTransport() const { return m_transport; }

    /* Actual session RTP transport. If user set 'automatic', it can be 'tcp' or 'udp',
     * otherwise it should be equal to result of getTransport().
     */
    nx::vms::api::RtpTransportType getActualTransport() const { return m_actualTransport; }

    const std::vector<SDPTrackInfo>& getTrackInfo() const;
    QString getTrackCodec(int rtpChannelNum);
    int getTrackNum(int rtpChannelNum);
    bool isRtcp(int rtpChannelNum);

    qint64 startTime() const;
    qint64 endTime() const;
    float getScale() const;
    void setScale(float value);

    bool sendSetup();
    bool sendPlay(qint64 startPos, qint64 endPos, double scale);
    bool sendPause();
    bool sendSetParameter(const QByteArray& paramName, const QByteArray& paramValue);
    bool sendTeardown();

    int lastSendedCSeq() const { return m_csec-1; }

    void setAdditionAttribute(const QByteArray& name, const QByteArray& value);
    void removeAdditionAttribute(const QByteArray& name);
    void setPreferredMap(const nx::rtp::Sdp::RtpMap& map);

    std::chrono::milliseconds getTCPTimeout() const;

    void parseRangeHeader(const QString& rangeStr);

    /*
    * Client will use any AuthScheme corresponding to server requirements (authenticate server request)
    * defaultAuthScheme is used for first client request only
    */
    void setUsernameAndPassword(
        const QString& username,
        const QString& password,
        nx::network::http::header::AuthScheme::Value defaultAuthScheme =
            nx::network::http::header::AuthScheme::basic);

    void setCredentials(const nx::network::http::Credentials& credentials,
        nx::network::http::header::AuthScheme::Value defaultAuthScheme = nx::network::http::header::AuthScheme::none);

    const nx::network::http::Credentials& getCredentials() const;

    nx::utils::Url getUrl() const;

    void setProxyAddr(const nx::String& addr, int port);

    QStringList getSdpByType(nx::rtp::Sdp::MediaType mediaType) const;
    int getTrackCount(nx::rtp::Sdp::MediaType mediaType) const;

    // Control what media streams will be opened during setup. All types enabled by default
    void setMediaTypeEnabled(nx::rtp::Sdp::MediaType mediaType, bool enabled);
    bool isMediaTypeEnabled(nx::rtp::Sdp::MediaType mediaType) const;

    /*
    * Demux RTSP binary data
    * @param data Buffer to write demuxed data. 4 byte RTSP header keep in buffer
    * @param maxDataSize maximum buffer size
    * @return amount of read bytes
    */
    int readBinaryResponse(quint8 *data, int maxDataSize);

    /*
    * Demux RTSP binary data.
    * @param demuxedData vector of buffers where stored demuxed data. Buffer number determined by RTSP channel number. 4 byte RTSP header are not stored in buffer
    * @param channelNumber buffer number
    * @return amount of read bytes
    */
    int readBinaryResponse(std::vector<nx::utils::ByteArray*>& demuxedData, int& channelNumber);

    void sendBynaryResponse(const quint8* buffer, int size);
    void setPlayNowModeAllowed(bool value);
    static quint8* prepareDemuxedData(std::vector<nx::utils::ByteArray*>& demuxedData, int channel, int reserve);

    QString getVideoLayout() const;
    nx::network::AbstractStreamSocket* tcpSock(); //< This method need for UT. do not delete
    void setUserAgent(const QString& value);

    /** @return "Server" http header value */
    QByteArray serverInfo() const;

    void setDateTimeFormat(const DateTimeFormat& format);
    void setScaleHeaderEnabled(bool value);
    void addRequestHeader(const QString& requestName, const nx::network::http::HttpHeader& header);

    bool processTcpRtcpData(const quint8* data, int size);

    QElapsedTimer lastReceivedDataTimer() const;

    bool parseSetupResponse(const QString& response, SDPTrackInfo* outTrack, int trackIndex);
    std::chrono::milliseconds keepAliveTimeOut() const { return m_keepAliveTimeOut; }
    QString sessionId() const { return m_SessionId; }

    /*
    * Some cameras declare qop support in digest authentication, but actually don't support it.
    */
    void setIgnoreQopInDigest(bool value);
    bool ignoreQopInDigest(bool value) const;

    bool readAndProcessTextData();
    void setCloudConnectEnabled(bool enabled);

private:
    void addRangeHeader( nx::network::http::Request* const request, qint64 startPos, qint64 endPos );
    nx::network::http::Request createDescribeRequest();
    CameraDiagnostics::Result sendOptions();
    CameraDiagnostics::Result sendDescribe();
    bool sendKeepAlive();

    bool readTextResponse(QByteArray &response);
    void fillRequestAuthorization(nx::network::http::Request* request);
    void fillRequestAuthorizationByResponseAuthenticate(nx::network::http::Request* request);

    void updateTransportHeader(QByteArray &response);

    bool parseSDP(const QByteArray& response);

    void addAdditionAttrs( nx::network::http::Request* const request );
    void processTextData(const QByteArray& textData);
    static QByteArray getGuid();
    void registerRTPChannel(int rtpNum, int rtcpNum, int trackIndex);
    nx::network::http::Request createPlayRequest( qint64 startPos, qint64 endPos );
    bool sendRequestInternal(nx::network::http::Request&& request);
    void addCommonHeaders(nx::network::http::HttpHeaders& headers);
    void addAdditionalHeaders(const QString& requestName, nx::network::http::HttpHeaders* outHeaders);
    QString parseContentBase(const QString& buffer);

private:
    enum { RTSP_BUFFER_LEN = 1024 * 65 };

    const Config m_config;

    // 'initialization in order' block
    unsigned int m_csec;
    nx::vms::api::RtpTransportType m_transport;
    int m_selectedAudioChannel;
    qint64 m_startTime;
    qint64 m_openedTime;
    qint64 m_endTime;
    float m_scale;
    std::chrono::milliseconds m_tcpTimeout;
    int m_numOfPredefinedChannels;
    std::chrono::milliseconds m_keepAliveTimeOut{0};
    bool m_playNowMode = false;
    // end of initialized fields

    std::set<nx::rtp::Sdp::MediaType> m_disabledMediaTypes;

    quint8* m_responseBuffer;
    int m_responseBufferLen;
    nx::rtp::Sdp m_sdp;

    std::unique_ptr<nx::network::AbstractStreamSocket> m_tcpSock;
    nx::utils::Url m_url;
    QString m_SessionId;
    unsigned short m_ServerPort;
    std::vector<SDPTrackInfo> m_sdpTracks;

    QElapsedTimer m_keepAliveTime;

    friend class QnRtspIoDevice;
    QMap<QByteArray, QByteArray> m_additionAttrs;
    nx::network::http::Credentials m_credentials;
    std::optional<nx::network::SocketAddress> m_proxyAddress;
    QString m_contentBase;
    nx::vms::api::RtpTransportType m_actualTransport;

    struct RtpChannel
    {
        bool isRtcp = false;
        int trackIndex = 0;
    };

    std::vector<RtpChannel> m_rtpToTrack;
    QString m_videoLayout;

    bool m_isProxyAuthorizationRequired = false;
    std::optional<nx::network::http::header::WWWAuthenticate> m_responseAuthenticate = std::nullopt;
    nx::String m_userAgent;
    nx::network::http::header::AuthScheme::Value m_defaultAuthScheme;
    mutable nx::Mutex m_socketMutex;
    nx::String m_serverInfo;
    DateTimeFormat m_dateTimeFormat = DateTimeFormat::Numeric;
    bool m_scaleHeaderEnabled = true;
    using RequestName = QString;
    QMap<RequestName, nx::network::http::HttpHeaders> m_additionalHeaders;
    QElapsedTimer m_lastReceivedDataTimer;
    std::set<QString> m_additionalSupportedCodecs;
    int m_tcpRecvBufferSize = 512 * 1024;
    int m_nonceCount = 0;
    bool m_ignoreQopInDigest = false;
    nx::network::NatTraversalSupport m_natTraversal{nx::network::NatTraversalSupport::enabled};
    bool m_keepAliveSupported = true;

    /*!
        \param readSome if \a true, returns as soon as some data has been read. Otherwise, blocks till all \a bufSize bytes has been read
    */
    int readSocketWithBuffering( quint8* buf, size_t bufSize, bool readSome );

    //!Sends request and waits for response. If request requires authentication, re-tries request with credentials
    /*!
        Updates \a m_responseCode member.
        \return error description
    */
    CameraDiagnostics::Result sendRequestAndReceiveResponse(nx::network::http::Request&& request, QByteArray& response);
};
