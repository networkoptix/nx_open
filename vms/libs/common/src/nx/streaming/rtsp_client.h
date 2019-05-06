#pragma once

#include <memory>
#include <vector>
#include <chrono>
#include <optional>

#include <QtNetwork/QAuthenticator>

extern "C"
{
// For const AV_NOPTS_VALUE.
#include <libavutil/avutil.h>
}

#include <QtCore/QDateTime>
#include <QtCore/QElapsedTimer>
#include <QtCore/QUrl>

#include <nx/network/socket.h>
#include <nx/network/rtsp/rtsp_types.h>
#include <nx/streaming/rtp/rtcp.h>
#include <nx/streaming/sdp.h>
#include <nx/utils/elapsed_timer.h>

#include <utils/common/threadqueue.h>
#include <utils/common/byte_array.h>
#include <utils/camera/camera_diagnostics.h>
#include <network/client_authenticate_helper.h>

#include <nx/vms/api/types/rtp_types.h>

class QnRtspClient;

static const int MAX_RTCP_PACKET_SIZE = 1024 * 2;
static const int MAX_RTP_PACKET_SIZE = 1024 * 16;

class QnRtspIoDevice
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
    const nx::streaming::rtp::RtcpSenderReport& getSenderReport() { return m_senderReport; }
    void setSenderReport(const nx::streaming::rtp::RtcpSenderReport& value) { m_senderReport = value; }
    nx::network::AbstractCommunicatingSocket* getMediaSocket();
    nx::network::AbstractDatagramSocket* getRtcpSocket() const { return m_rtcpSocket.get(); }
    void shutdown();
    void setTransport(nx::vms::api::RtpTransportType rtpTransport);
    void setSSRC(quint32 value) {ssrc = value; }
    quint32 getSSRC() const { return ssrc; }

    void setRemoteEndpointRtcpPort(quint16 rtcpPort) {m_remoteRtcpPort = rtcpPort;}

    void updateRemoteMulticastPorts(quint16 mediaPort, quint16 rtcpPort);
    void setHostAddress(const nx::network::HostAddress& hostAddress) {m_hostAddress = hostAddress;};
    void setForceRtcpReports(bool force) {m_forceRtcpReports = force;};


    void bindToMulticastAddress(const QHostAddress& address, const QString& interfaceAddress);

    AddressInfo mediaAddressInfo() const;
    AddressInfo rtcpAddressInfo() const;

private:
    AddressInfo addressInfo(int port) const;
    void processRtcpData();
    void updateSockets();

private:
    QnRtspClient* m_owner = nullptr;
    nx::vms::api::RtpTransportType m_transport = nx::vms::api::RtpTransportType::automatic;

    nx::streaming::rtp::RtcpSenderReport m_senderReport;
    std::unique_ptr<nx::network::AbstractDatagramSocket> m_mediaSocket;
    std::unique_ptr< nx::network::AbstractDatagramSocket> m_rtcpSocket;
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

class QnRtspClient
{
public:
    struct Config
    {
        bool shouldGuessAuthDigest = false;
        bool backChannelAudioOnly = false;
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
            const nx::streaming::Sdp::Media& sdpMedia,
            QnRtspClient* owner,
            nx::vms::api::RtpTransportType transport,
            int serverPort)
            :
            sdpMedia(sdpMedia)
        {
            ioDevice = std::make_shared<QnRtspIoDevice>(owner, transport, serverPort);
            ioDevice->setHostAddress(nx::network::HostAddress(owner->getUrl().host()));
        }
        void setRemoteEndpointRtcpPort(quint16 rtcpPort) { ioDevice->setRemoteEndpointRtcpPort(rtcpPort); };

        bool setupSuccess = false;
        nx::streaming::Sdp::Media sdpMedia;
        QPair<int, int> interleaved{ -1, -1 };
        std::shared_ptr<QnRtspIoDevice> ioDevice;
    };
    QnRtspClient(const Config& config,
        std::unique_ptr<nx::network::AbstractStreamSocket> tcpSock = std::unique_ptr<nx::network::AbstractStreamSocket>());

    ~QnRtspClient();

    // returns \a CameraDiagnostics::ErrorCode::noError if stream was opened, error code - otherwise
    CameraDiagnostics::Result open(const nx::utils::Url& url, qint64 startTime = AV_NOPTS_VALUE);

    /*
    * Start playing RTSP sessopn.
    * @param positionStart start position at mksec
    * @param positionEnd end position at mksec
    * @param scale playback speed
    */
    bool play(qint64 positionStart, qint64 positionEnd, double scale);

    // returns true if there is no error delivering STOP
    bool stop();

    /** Shutdown TCP socket and terminate current IO operation. Can be called from the other thread */
    void shutdown();

    // returns true if session is opened
    bool isOpened() const;

    // session timeout in ms
    unsigned int sessionTimeoutMs();

    const nx::streaming::Sdp& getSdp() const;

    void setKeepAliveTimeout(std::chrono::milliseconds keepAliveTimeout);

    bool sendKeepAliveIfNeeded();

    void setTransport(nx::vms::api::RtpTransportType transport);

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

    void setTCPTimeout(std::chrono::milliseconds timeout);
    std::chrono::milliseconds getTCPTimeout() const;

    void parseRangeHeader(const QString& rangeStr);

    /*
    * Client will use any AuthScheme corresponding to server requirements (authenticate server request)
    * defaultAuthScheme is used for first client request only
    */
    void setAuth(const QAuthenticator& auth, nx::network::http::header::AuthScheme::Value defaultAuthScheme);
    QAuthenticator getAuth() const;

    nx::utils::Url getUrl() const;

    void setProxyAddr(const QString& addr, int port);

    QStringList getSdpByType(nx::streaming::Sdp::MediaType mediaType) const;
    int getTrackCount(nx::streaming::Sdp::MediaType mediaType) const;

    nx::network::rtsp::StatusCodeValue getLastResponseCode() const;

    void setAudioEnabled(bool value);
    bool isAudioEnabled() const;

    /*
    * Demuxe RTSP binary data
    * @param data Buffer to write demuxed data. 4 byte RTSP header keep in buffer
    * @param maxDataSize maximum buffer size
    * @return amount of readed bytes
    */
    int readBinaryResponce(quint8 *data, int maxDataSize);

    /*
    * Demuxe RTSP binary data.
    * @param demuxedData vector of buffers where stored demuxed data. Buffer number determined by RTSP channel number. 4 byte RTSP header are not stored in buffer
    * @param channelNumber buffer number
    * @return amount of readed bytes
    */
    int readBinaryResponce(std::vector<QnByteArray*>& demuxedData, int& channelNumber);

    void sendBynaryResponse(const quint8* buffer, int size);
    void setPlayNowModeAllowed(bool value);
    static quint8* prepareDemuxedData(std::vector<QnByteArray*>& demuxedData, int channel, int reserve);

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

private:
    void addRangeHeader( nx::network::http::Request* const request, qint64 startPos, qint64 endPos );
    nx::network::http::Request createDescribeRequest();
    bool sendDescribe();
    bool sendOptions();
    bool sendKeepAlive();
    bool sendSetupIfNotPlaying();

    bool readTextResponce(QByteArray &responce);
    void addAuth( nx::network::http::Request* const request );

    QString extractRTSPParam(const QString &buffer, const QString &paramName);
    void updateTransportHeader(QByteArray &responce);

    void parseSDP(const QByteArray& data);

    void addAdditionAttrs( nx::network::http::Request* const request );
    void updateResponseStatus(const QByteArray& response);

    bool processTextResponseInsideBinData();
    static QByteArray getGuid();
    void registerRTPChannel(int rtpNum, int rtcpNum, int trackIndex);
    nx::network::http::Request createPlayRequest( qint64 startPos, qint64 endPos );
    bool sendRequestInternal(nx::network::http::Request&& request);
    void addCommonHeaders(nx::network::http::HttpHeaders& headers);
    void addAdditionalHeaders(const QString& requestName, nx::network::http::HttpHeaders* outHeaders);

    QByteArray nptPosToString(qint64 posUsec) const;
private:
    enum { RTSP_BUFFER_LEN = 1024 * 65 };

    Config m_config;

    // 'initialization in order' block
    unsigned int m_csec;
    nx::vms::api::RtpTransportType m_transport;
    int m_selectedAudioChannel;
    qint64 m_startTime;
    qint64 m_openedTime;
    qint64 m_endTime;
    float m_scale;
    std::chrono::milliseconds m_tcpTimeout;
    nx::network::rtsp::StatusCodeValue m_responseCode;
    bool m_isAudioEnabled;
    int m_numOfPredefinedChannels;
    std::chrono::milliseconds m_keepAliveTimeOut{0};
    bool m_playNowMode = false;
    // end of initialized fields

    quint8* m_responseBuffer;
    int m_responseBufferLen;
    nx::streaming::Sdp m_sdp;

    std::unique_ptr<nx::network::AbstractStreamSocket> m_tcpSock;
    nx::utils::Url m_url;
    QString m_SessionId;
    unsigned short m_ServerPort;
    std::vector<SDPTrackInfo> m_sdpTracks;

    QElapsedTimer m_keepAliveTime;

    friend class QnRtspIoDevice;
    QMap<QByteArray, QByteArray> m_additionAttrs;
    QAuthenticator m_auth;
    boost::optional<nx::network::SocketAddress> m_proxyAddress;
    QString m_contentBase;
    nx::vms::api::RtpTransportType m_actualTransport;

    static QByteArray m_guid; // client guid. used in proprietary extension
    static QnMutex m_guidMutex;

    struct RtpChannel
    {
        bool isRtcp = false;
        int trackIndex = 0;
    };

    std::vector<RtpChannel> m_rtpToTrack;
    QString m_reasonPhrase;
    QString m_videoLayout;

    HttpAuthenticationClientContext m_rtspAuthCtx;
    QByteArray m_userAgent;
    nx::network::http::header::AuthScheme::Value m_defaultAuthScheme;
    mutable QnMutex m_socketMutex;
    QByteArray m_serverInfo;
    DateTimeFormat m_dateTimeFormat = DateTimeFormat::Numeric;
    bool m_scaleHeaderEnabled = true;
    using RequestName = QString;
    QMap<RequestName, nx::network::http::HttpHeaders> m_additionalHeaders;
    QElapsedTimer m_lastReceivedDataTimer;

    /*!
        \param readSome if \a true, returns as soon as some data has been read. Otherwise, blocks till all \a bufSize bytes has been read
    */
    int readSocketWithBuffering( quint8* buf, size_t bufSize, bool readSome );

    //!Sends request and waits for response. If request requires authentication, re-tries request with credentials
    /*!
        Updates \a m_responseCode member.
        \return error description
    */
    bool sendRequestAndReceiveResponse( nx::network::http::Request&& request, QByteArray& responce );
};
