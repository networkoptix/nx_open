#pragma once

#include <QtCore/QElapsedTimer>

#include <fstream>
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
#include <nx/network/http/http_types.h>
#include <nx/network/rtsp/rtsp_types.h>
#include <nx/streaming/rtp/rtcp.h>
#include <nx/streaming/rtp/camera_time_helper.h>


#include <utils/common/threadqueue.h>
#include <utils/common/byte_array.h>
#include <utils/camera/camera_diagnostics.h>
#include <network/client_authenticate_helper.h>
#include <nx/utils/elapsed_timer.h>

//#define DEBUG_TIMINGS
//#define _DUMP_STREAM

class QnRtspClient;

static const int MAX_RTCP_PACKET_SIZE = 1024 * 2;

static const int RTSP_FFMPEG_GENERIC_HEADER_SIZE = 8;
static const int RTSP_FFMPEG_VIDEO_HEADER_SIZE = 3;
static const int RTSP_FFMPEG_METADATA_HEADER_SIZE = 8; //< m_duration + metadataType
static const int RTSP_FFMPEG_MAX_HEADER_SIZE = RTSP_FFMPEG_GENERIC_HEADER_SIZE + RTSP_FFMPEG_METADATA_HEADER_SIZE;
static const int MAX_RTP_PACKET_SIZE = 1024 * 16;

class QnRtspIoDevice
{
public:
    explicit QnRtspIoDevice(QnRtspClient* owner, bool useTCP, quint16 mediaPort = 0, quint16 rtcpPort = 0);
    virtual ~QnRtspIoDevice();
    virtual qint64 read(char * data, qint64 maxSize );
    const nx::streaming::rtp::RtcpSenderReport& getSenderReport() { return m_senderReport; }
    void setSenderReport(const nx::streaming::rtp::RtcpSenderReport& value) { m_senderReport = value; }
    nx::network::AbstractCommunicatingSocket* getMediaSocket();
    nx::network::AbstractDatagramSocket* getRtcpSocket() const { return m_rtcpSocket; }
    void shutdown();
    void setTcpMode(bool value);
    void setSSRC(quint32 value) {ssrc = value; }
    quint32 getSSRC() const { return ssrc; }

    void setRemoteEndpointRtcpPort(quint16 rtcpPort) {m_remoteEndpointRtcpPort = rtcpPort;}
    void setHostAddress(const nx::network::HostAddress& hostAddress) {m_hostAddress = hostAddress;};
    void setForceRtcpReports(bool force) {m_forceRtcpReports = force;};

private:
    void processRtcpData();

private:
    QnRtspClient* m_owner;
    bool m_tcpMode;
    nx::streaming::rtp::RtcpSenderReport m_senderReport;
    nx::network::AbstractDatagramSocket* m_mediaSocket;
    nx::network::AbstractDatagramSocket* m_rtcpSocket;
    quint16 m_mediaPort;
    quint16 m_remoteEndpointRtcpPort;
    nx::network::HostAddress m_hostAddress;
    quint32 ssrc;
    QElapsedTimer m_reportTimer;
    bool m_reportTimerStarted;
    bool m_forceRtcpReports;
};

class QnRtspClient: public QObject
{
    Q_OBJECT;
public:
    //typedef QMap<int, QScopedPointer<QnRtspIoDevice> > RtpIoTracks;

    enum TrackType {TT_VIDEO, TT_VIDEO_RTCP, TT_AUDIO, TT_AUDIO_RTCP, TT_METADATA, TT_METADATA_RTCP, TT_UNKNOWN, TT_UNKNOWN2};
    enum TransportType {TRANSPORT_UDP, TRANSPORT_TCP, TRANSPORT_AUTO };

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
        SDPTrackInfo(
            const QString& codecName,
            const QByteArray& trackTypeStr,
            const QByteArray& setupURL,
            int mapNumber,
            QnRtspClient* owner,
            bool useTCP)
            :
            SDPTrackInfo(owner, useTCP)
        {
            this->codecName = codecName;
            this->trackType = trackTypeFromString(trackTypeStr);
            this->setupURL = setupURL;
            this->mapNumber = mapNumber;
        }
        SDPTrackInfo(QnRtspClient* owner, bool useTCP)
        {
            ioDevice = new QnRtspIoDevice(owner, useTCP);
            ioDevice->setHostAddress(nx::network::HostAddress(owner->getUrl().host()));
        }
        ~SDPTrackInfo() { delete ioDevice; }

        static TrackType trackTypeFromString(const QByteArray& value)
        {
            QByteArray trackTypeStr = value.toLower();
            if (trackTypeStr == "audio")
                return TT_AUDIO;
            else if (trackTypeStr == "audio-rtcp")
                return TT_AUDIO_RTCP;
            else if (trackTypeStr == "video")
                return TT_VIDEO;
            else if (trackTypeStr == "video-rtcp")
                return TT_VIDEO_RTCP;
            else if (trackTypeStr == "metadata")
                return TT_METADATA;
            else
                return TT_UNKNOWN;
        }

        void setMapNumber(int value)
        {
            mapNumber = value;
            if (codecName.isEmpty())
                codecName = findCodecById(mapNumber);
        }

        // see rfc1890 for full RTP predefined codec list
        static QString findCodecById(int num)
        {
            switch (num)
            {
            case 0: return QLatin1String("PCMU");
            case 8: return QLatin1String("PCMA");
            case 26: return QLatin1String("JPEG");
            default: return QString();
            }
        }

        bool isValid() const
        {
            return mapNumber >= 0 && !codecName.isEmpty();
        }

        void setRemoteEndpointRtcpPort(quint16 rtcpPort) { ioDevice->setRemoteEndpointRtcpPort(rtcpPort); };

        void setSSRC(quint32 value);
        quint32 getSSRC() const;

        QString codecName;
        TrackType trackType = TT_UNKNOWN;
        QByteArray setupURL;
        int mapNumber = -1;
        int trackNumber = -1;
        QPair<int, int> interleaved{ -1, -1 };
        QnRtspIoDevice* ioDevice = nullptr;
        bool isBackChannel = false;
        int timeBase = 0;
    };

    static QString mediaTypeToStr(TrackType tt);

    //typedef QMap<int, QSharedPointer<SDPTrackInfo> > TrackMap;
    typedef QVector<QSharedPointer<SDPTrackInfo> > TrackMap;

    QnRtspClient(
        bool shouldGuessAuthDigest,
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
    QnRtspClient::TrackMap play(qint64 positionStart, qint64 positionEnd, double scale);

    // returns true if there is no error delivering STOP
    bool stop();

    /** Shutdown TCP socket and terminate current IO operation. Can be called from the other thread */
    void shutdown();

    // returns true if session is opened
    bool isOpened() const;

    // session timeout in ms
    unsigned int sessionTimeoutMs();

    const QByteArray& getSdp() const;

    bool sendKeepAliveIfNeeded();

    void setTransport(TransportType transport);
    void setTransport(const QString& transport);
    TransportType getTransport() const { return m_transport; }
    bool isTcpMode() const { return m_prefferedTransport != TRANSPORT_UDP; }
    QString getTrackFormatByRtpChannelNum(int channelNum);
    TrackType getTrackTypeByRtpChannelNum(int channelNum);
    int getChannelNum(int rtpChannelNum);

    qint64 startTime() const;
    qint64 endTime() const;
    float getScale() const;
    void setScale(float value);

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

    QList<QByteArray> getSdpByTrackNum(int trackNum) const;
    QList<QByteArray> getSdpByType(TrackType trackType) const;
    int getTrackCount(TrackType trackType) const;

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
    void setUsePredefinedTracks(int numOfVideoChannel);
    static quint8* prepareDemuxedData(std::vector<QnByteArray*>& demuxedData, int channel, int reserve);
    bool setTCPReadBufferSize(int value);

    QString getVideoLayout() const;
    TrackMap getTrackInfo() const;

    void setTrackInfo(const TrackMap& tracks);

    nx::network::AbstractStreamSocket* tcpSock(); //< This method need for UT. do not delete
    void setUserAgent(const QString& value);

    /** @return "Server" http header value */
    QByteArray serverInfo() const;

    void setDateTimeFormat(const DateTimeFormat& format);
    void setScaleHeaderEnabled(bool value);
    void addRequestHeader(const QString& requestName, const nx::network::http::HttpHeader& header);

    bool processTcpRtcpData(const quint8* data, int size);

    QElapsedTimer lastReceivedDataTimer() const;
signals:
    void gotTextResponse(QByteArray text);
private:
    void addRangeHeader( nx::network::http::Request* const request, qint64 startPos, qint64 endPos );
    QString getTrackFormat(int trackNum) const;
    TrackType getTrackType(int trackNum) const;
    //int readRAWData();
    nx::network::http::Request createDescribeRequest();
    bool sendDescribe();
    bool sendOptions();
    bool sendSetup();
    bool sendKeepAlive();

    bool readTextResponce(QByteArray &responce);
    void addAuth( nx::network::http::Request* const request );

    QString extractRTSPParam(const QString &buffer, const QString &paramName);
    void updateTransportHeader(QByteArray &responce);

    void parseSDP();
    void updateTrackNum();
    void addAdditionAttrs( nx::network::http::Request* const request );
    void updateResponseStatus(const QByteArray& response);

    void usePredefinedTracks();
    bool processTextResponseInsideBinData();
    static QByteArray getGuid();
    void registerRTPChannel(int rtpNum, QSharedPointer<SDPTrackInfo> trackInfo);
    QByteArray calcDefaultNonce() const;
    nx::network::http::Request createPlayRequest( qint64 startPos, qint64 endPos );
    bool sendPlayInternal(qint64 startPos, qint64 endPos);
    bool sendRequestInternal(nx::network::http::Request&& request);
    void addCommonHeaders(nx::network::http::HttpHeaders& headers);
    void addAdditionalHeaders(const QString& requestName, nx::network::http::HttpHeaders* outHeaders);

    QByteArray nptPosToString(qint64 posUsec) const;
private:
    enum { RTSP_BUFFER_LEN = 1024 * 65 };

    // 'initialization in order' block
    unsigned int m_csec;
    TransportType m_transport;
    int m_selectedAudioChannel;
    qint64 m_startTime;
    qint64 m_openedTime;
    qint64 m_endTime;
    float m_scale;
    std::chrono::milliseconds m_tcpTimeout;
    nx::network::rtsp::StatusCodeValue m_responseCode;
    bool m_isAudioEnabled;
    int m_numOfPredefinedChannels;
    unsigned int m_TimeOut;
    // end of initialized fields

    //unsigned char m_responseBuffer[MAX_RESPONCE_LEN];
    quint8* m_responseBuffer;
    int m_responseBufferLen;
    QByteArray m_sdp;

    std::unique_ptr<nx::network::AbstractStreamSocket> m_tcpSock;
    //RtpIoTracks m_rtpIoTracks; // key: tracknum, value: track IO device

    nx::utils::Url m_url;

    QString m_SessionId;
    unsigned short m_ServerPort;
    // format: key - track number, value - codec name
    TrackMap m_sdpTracks;

    QElapsedTimer m_keepAliveTime;

    friend class QnRtspIoDevice;
    QMap<QByteArray, QByteArray> m_additionAttrs;
    QAuthenticator m_auth;
    boost::optional<nx::network::SocketAddress> m_proxyAddress;
    QString m_contentBase;
    TransportType m_prefferedTransport;

    static QByteArray m_guid; // client guid. used in proprietary extension
    static QnMutex m_guidMutex;

    std::vector<QSharedPointer<SDPTrackInfo> > m_rtpToTrack;
    QString m_reasonPhrase;
    QString m_videoLayout;

    char* m_additionalReadBuffer;
    int m_additionalReadBufferPos;
    int m_additionalReadBufferSize;
    HttpAuthenticationClientContext m_rtspAuthCtx;
    QByteArray m_userAgent;
#ifdef _DUMP_STREAM
    std::ofstream m_inStreamFile;
    std::ofstream m_outStreamFile;
#endif
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
