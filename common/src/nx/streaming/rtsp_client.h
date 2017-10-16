#ifndef RTSP_CLIENT_H
#define RTSP_CLIENT_H

#include <fstream>
#include <memory>
#include <vector>

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

#include <utils/common/threadqueue.h>
#include <utils/common/byte_array.h>
#include <utils/camera/camera_diagnostics.h>
#include <network/client_authenticate_helper.h>

//#define DEBUG_TIMINGS
//#define _DUMP_STREAM

class QnRtspClient;

static const int RTSP_FFMPEG_GENERIC_HEADER_SIZE = 8;
static const int RTSP_FFMPEG_VIDEO_HEADER_SIZE = 3;
static const int RTSP_FFMPEG_METADATA_HEADER_SIZE = 8; //< m_duration + metadataType
static const int RTSP_FFMPEG_MAX_HEADER_SIZE = RTSP_FFMPEG_GENERIC_HEADER_SIZE + RTSP_FFMPEG_METADATA_HEADER_SIZE;
static const int MAX_RTP_PACKET_SIZE = 1024 * 16;

class QnRtspStatistic
{
public:
    QnRtspStatistic(): timestamp(0), ntpTime(0), localTime(0), receivedPackets(0), receivedOctets(0), ssrc(0) {}
    bool isEmpty() const { return timestamp == 0 && ntpTime == 0; }

    quint32 timestamp;
    double ntpTime;
    double localTime;
    qint64 receivedPackets;
    qint64 receivedOctets;
    quint32 ssrc;
};

enum class TimePolicy
{
    BindCameraTimeToLocalTime, //< Use camera NPT time, bind it to local time.
    IgnoreCameraTimeIfBigJitter, //< Same as previous, switch to ForceLocalTime if big jitter.
    ForceLocalTime, //< Use local time only.
    ForceCameraTime //< Use camera NPT time only.
};

class QnRtspTimeHelper
{
public:

    QnRtspTimeHelper(const QString& resId);
    ~QnRtspTimeHelper();

    /*!
        \note Overflow of \a rtpTime is not handled here, so be sure to update \a statistics often enough (twice per \a rtpTime full cycle)
    */
    qint64 getUsecTime(quint32 rtpTime, const QnRtspStatistic& statistics, int rtpFrequency, bool recursionAllowed = true);
    QString getResID() const { return m_resourceId; }

    void setTimePolicy(TimePolicy policy);
private:
    double cameraTimeToLocalTime(double cameraSecondsSinceEpoch, double currentSecondsSinceEpoch);
    bool isLocalTimeChanged();
    bool isCameraTimeChanged(const QnRtspStatistic& statistics);
    void reset();
private:
    quint32 m_prevRtpTime = 0;
    quint32 m_prevCurrentSeconds = 0;
    QElapsedTimer m_timer;
    qint64 m_localStartTime;
    //qint64 m_cameraTimeDrift;
    double m_rtcpReportTimeDiff;

    struct CamSyncInfo {
        CamSyncInfo(): timeDiff(INT_MAX) {}
        QnMutex mutex;
        double timeDiff;
    };

    QSharedPointer<CamSyncInfo> m_cameraClockToLocalDiff;
    QString m_resourceId;

    static QnMutex m_camClockMutex;
    static QMap<QString, QPair<QSharedPointer<QnRtspTimeHelper::CamSyncInfo>, int> > m_camClock;
    qint64 m_lastWarnTime;
    TimePolicy m_timePolicy = TimePolicy::BindCameraTimeToLocalTime;

#ifdef DEBUG_TIMINGS
    void printTime(double jitter);
    QElapsedTimer m_statsTimer;
    double m_minJitter;
    double m_maxJitter;
    double m_jitterSum;
    int m_jitPackets;
#endif
};

class QnRtspIoDevice
{
public:
    explicit QnRtspIoDevice(QnRtspClient* owner, bool useTCP, quint16 mediaPort = 0, quint16 rtcpPort = 0);
    virtual ~QnRtspIoDevice();
    virtual qint64 read(char * data, qint64 maxSize );
    const QnRtspStatistic& getStatistic() { return m_statistic; }
    void setStatistic(const QnRtspStatistic& value) { m_statistic = value; }
    AbstractCommunicatingSocket* getMediaSocket();
    AbstractDatagramSocket* getRtcpSocket() const { return m_rtcpSocket; }
    void shutdown();
    void setTcpMode(bool value);
    void setSSRC(quint32 value) {ssrc = value; }
    quint32 getSSRC() const { return ssrc; }

    void setRtpTrackNum(quint8 value) { m_rtpTrackNum = value; }
    void setRemoteEndpointRtcpPort(quint16 rtcpPort) {m_remoteEndpointRtcpPort = rtcpPort;}
    void setHostAddress(const HostAddress& hostAddress) {m_hostAddress = hostAddress;};
    void setForceRtcpReports(bool force) {m_forceRtcpReports = force;};
    quint8 getRtpTrackNum() const { return m_rtpTrackNum; }
    quint8 getRtcpTrackNum() const { return m_rtpTrackNum+1; }
private:
    void processRtcpData();
private:
    QnRtspClient* m_owner;
    bool m_tcpMode;
    QnRtspStatistic m_statistic;
    AbstractDatagramSocket* m_mediaSocket;
    AbstractDatagramSocket* m_rtcpSocket;
    quint16 m_mediaPort;
    quint16 m_remoteEndpointRtcpPort;
    HostAddress m_hostAddress;
    quint32 ssrc;
    quint8 m_rtpTrackNum;
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

    enum class DateTimeFormat
    {
        Numeric,
        ISO
    };

    struct SDPTrackInfo
    {
        SDPTrackInfo(const QString& _codecName, const QByteArray& _trackTypeStr, const QByteArray& _setupURL, int _mapNum, int _trackNum, QnRtspClient* owner, bool useTCP):
            codecName(_codecName), setupURL(_setupURL), mapNum(_mapNum), trackNum(_trackNum)
        {
            QByteArray trackTypeStr = _trackTypeStr.toLower();
            if (trackTypeStr == "audio")
                trackType = TT_AUDIO;
            else if (trackTypeStr == "audio-rtcp")
                trackType = TT_AUDIO_RTCP;
            else if (trackTypeStr == "video")
                trackType = TT_VIDEO;
            else if (trackTypeStr == "video-rtcp")
                trackType = TT_VIDEO_RTCP;
            else if (trackTypeStr == "metadata")
                trackType = TT_METADATA;
            else
                trackType = TT_UNKNOWN;

            ioDevice = new QnRtspIoDevice(owner, useTCP);
            ioDevice->setRtpTrackNum(_trackNum * 2);
            ioDevice->setHostAddress(HostAddress(owner->getUrl().host()));
            interleaved = QPair<int,int>(-1,-1);
        }

        void setRemoteEndpointRtcpPort(quint16 rtcpPort) {ioDevice->setRemoteEndpointRtcpPort(rtcpPort);};

        void setSSRC(quint32 value);
        quint32 getSSRC() const;

        ~SDPTrackInfo() { delete ioDevice; }

        QString codecName;
        TrackType trackType;
        QByteArray setupURL;
        int mapNum;
        int trackNum;
        QPair<int,int> interleaved;

        QnRtspIoDevice* ioDevice;

        bool isBackChannel = false;
        int timeBase = 0;
    };

    static QString mediaTypeToStr(TrackType tt);

    //typedef QMap<int, QSharedPointer<SDPTrackInfo> > TrackMap;
    typedef QVector<QSharedPointer<SDPTrackInfo> > TrackMap;

    QnRtspClient(
        bool shouldGuessAuthDigest,
        std::unique_ptr<AbstractStreamSocket> tcpSock = std::unique_ptr<AbstractStreamSocket>());

    ~QnRtspClient();

    // returns \a CameraDiagnostics::ErrorCode::noError if stream was opened, error code - otherwise
    CameraDiagnostics::Result open(const QString& url, qint64 startTime = AV_NOPTS_VALUE);

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

    void setTCPTimeout(int timeout);

    void parseRangeHeader(const QString& rangeStr);

    /*
    * Client will use any AuthScheme corresponding to server requirements (authenticate server request)
    * defaultAuthScheme is used for first client request only
    */
    void setAuth(const QAuthenticator& auth, nx_http::header::AuthScheme::Value defaultAuthScheme);
    QAuthenticator getAuth() const;

    QUrl getUrl() const;

    void setProxyAddr(const QString& addr, int port);

    QList<QByteArray> getSdpByTrackNum(int trackNum) const;
    QList<QByteArray> getSdpByType(TrackType trackType) const;
    int getTrackCount(TrackType trackType) const;

    int getLastResponseCode() const;

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

    QnRtspStatistic parseServerRTCPReport(quint8* srcBuffer, int srcBufferSize, bool* gotStatistics);
    int buildClientRTCPReport(quint8 *dstBuffer, int bufferLen);

    void setUsePredefinedTracks(int numOfVideoChannel);

    static quint8* prepareDemuxedData(std::vector<QnByteArray*>& demuxedData, int channel, int reserve);

    bool setTCPReadBufferSize(int value);

    QString getVideoLayout() const;
    TrackMap getTrackInfo() const;
    
    void setTrackInfo(const TrackMap& tracks);

    AbstractStreamSocket* tcpSock(); //< This method need for UT. do not delete
    void setUserAgent(const QString& value);

    /** @return "Server" http header value */
    QByteArray serverInfo() const;

    void setDateTimeFormat(const DateTimeFormat& format);
    void setScaleHeaderEnabled(bool value);
signals:
    void gotTextResponse(QByteArray text);
private:
    void addRangeHeader( nx_http::Request* const request, qint64 startPos, qint64 endPos );
    QString getTrackFormat(int trackNum) const;
    TrackType getTrackType(int trackNum) const;
    //int readRAWData();
    nx_http::Request createDescribeRequest();
    bool sendDescribe();
    bool sendOptions();
    bool sendSetup();
    bool sendKeepAlive();

    bool readTextResponce(QByteArray &responce);
    void addAuth( nx_http::Request* const request );


    QString extractRTSPParam(const QString &buffer, const QString &paramName);
    void updateTransportHeader(QByteArray &responce);

    void parseSDP();
    void updateTrackNum();
    void addAdditionAttrs( nx_http::Request* const request );
    void updateResponseStatus(const QByteArray& response);

    void usePredefinedTracks();
    bool processTextResponseInsideBinData();
    static QByteArray getGuid();
    void registerRTPChannel(int rtpNum, QSharedPointer<SDPTrackInfo> trackInfo);
    QByteArray calcDefaultNonce() const;
    nx_http::Request createPlayRequest( qint64 startPos, qint64 endPos );
    bool sendPlayInternal(qint64 startPos, qint64 endPos);
    bool sendRequestInternal(nx_http::Request&& request);
    void addCommonHeaders(nx_http::HttpHeaders& headers);
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
    int m_tcpTimeout;
    int m_responseCode;
    bool m_isAudioEnabled;
    int m_numOfPredefinedChannels;
    unsigned int m_TimeOut;
    // end of initialized fields

    //unsigned char m_responseBuffer[MAX_RESPONCE_LEN];
    quint8* m_responseBuffer;
    int m_responseBufferLen;
    QByteArray m_sdp;

    std::unique_ptr<AbstractStreamSocket> m_tcpSock;
    //RtpIoTracks m_rtpIoTracks; // key: tracknum, value: track IO device

    QUrl m_url;

    QString m_SessionId;
    unsigned short m_ServerPort;
    // format: key - track number, value - codec name
    TrackMap m_sdpTracks;

    QElapsedTimer m_keepAliveTime;

    friend class QnRtspIoDevice;
    QMap<QByteArray, QByteArray> m_additionAttrs;
    QAuthenticator m_auth;
    boost::optional<SocketAddress> m_proxyAddress;
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
    nx_http::header::AuthScheme::Value m_defaultAuthScheme;
    mutable QnMutex m_socketMutex;
    QByteArray m_serverInfo;
    DateTimeFormat m_dateTimeFormat = DateTimeFormat::Numeric;
    bool m_scaleHeaderEnabled = true;

    /*!
        \param readSome if \a true, returns as soon as some data has been read. Otherwise, blocks till all \a bufSize bytes has been read
    */
    int readSocketWithBuffering( quint8* buf, size_t bufSize, bool readSome );

    //!Sends request and waits for response. If request requires authentication, re-tries request with credentials
    /*!
        Updates \a m_responseCode member.
        \return error description
    */
    bool sendRequestAndReceiveResponse( nx_http::Request&& request, QByteArray& responce );
};

#endif // RTSP_CLIENT_H
