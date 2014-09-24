#ifndef rtp_session_h_1935_h
#define rtp_session_h_1935_h

#include <memory>
#include <vector>

#include <QAuthenticator>

#include "socket.h"

extern "C"
{
    #include <libavformat/avformat.h>
}

#include <QtCore/QDateTime>
#include <QtCore/QElapsedTimer>
#include <QtCore/QUrl>
#include "../common/threadqueue.h"
#include "utils/camera/camera_diagnostics.h"

//#define DEBUG_TIMINGS

class RTPSession;

static const int RTSP_FFMPEG_GENERIC_HEADER_SIZE = 8;
static const int RTSP_FFMPEG_VIDEO_HEADER_SIZE = 3;
static const int RTSP_FFMPEG_METADATA_HEADER_SIZE = 4;
static const int RTSP_FFMPEG_MAX_HEADER_SIZE = RTSP_FFMPEG_GENERIC_HEADER_SIZE + RTSP_FFMPEG_METADATA_HEADER_SIZE;
static const int MAX_RTP_PACKET_SIZE = 1024 * 16;

class RtspStatistic 
{
public:
    RtspStatistic(): timestamp(0), nptTime(0), localtime(0), receivedPackets(0), receivedOctets(0), ssrc(0) {}
    bool isEmpty() const { return timestamp == 0 && nptTime == 0; }

    quint32 timestamp;
    double nptTime;
    double localtime;
    qint64 receivedPackets;
    qint64 receivedOctets;
    quint32 ssrc;
};

class QnRtspTimeHelper
{
public:
    QnRtspTimeHelper(const QString& resId);
    ~QnRtspTimeHelper();

    /*!
        \note Overflow of \a rtpTime is not handled here, so be sure to update \a statistics often enough (twice per \a rtpTime full cycle)
    */
    qint64 getUsecTime(quint32 rtpTime, const RtspStatistic& statistics, int rtpFrequency, bool recursiveAllowed = true);
    QString getResID() const { return m_resId; }
private:
    double cameraTimeToLocalTime(double cameraTime); // time in seconds since 1.1.1970
    bool isLocalTimeChanged();
    bool isCameraTimeChanged(const RtspStatistic& statistics);
    void reset();
private:
    qint64 m_lastTime;
    //double m_lastResultInSec;
    QElapsedTimer m_timer;
    qint64 m_localStartTime;
    qint64 m_cameraTimeDrift;
    double m_rtcpReportTimeDiff;

    struct CamSyncInfo {
        CamSyncInfo(): timeDiff(INT_MAX), driftSum(0) {}
        QMutex mutex;
        double timeDiff;
        QnUnsafeQueue<qint64> driftStats;
        qint64 driftSum;
    };

    QSharedPointer<CamSyncInfo> m_cameraClockToLocalDiff;
    QString m_resId;

    static QMutex m_camClockMutex;
    static QMap<QString, QPair<QSharedPointer<QnRtspTimeHelper::CamSyncInfo>, int> > m_camClock;
    qint64 m_lastWarnTime;

#ifdef DEBUG_TIMINGS
    void printTime(double jitter);
    QElapsedTimer m_statsTimer;
    double m_minJitter;
    double m_maxJitter;
    double m_jitterSum;
    int m_jitPackets;
#endif
};

class RTPIODevice
{
public:
    explicit RTPIODevice(RTPSession* owner, bool useTCP);
    virtual ~RTPIODevice();
    virtual qint64 read(char * data, qint64 maxSize );
    
    const RtspStatistic& getStatistic() { return m_statistic;}
    void setStatistic(const RtspStatistic& value) { m_statistic = value; }
    AbstractCommunicatingSocket* getMediaSocket();
    AbstractDatagramSocket* getRtcpSocket() const { return m_rtcpSocket; }
    void setTcpMode(bool value);
    void setSSRC(quint32 value) {ssrc = value; }
    quint32 getSSRC() const { return ssrc; }
    
    void setRtpTrackNum(quint8 value) { m_rtpTrackNum = value; }
    quint8 getRtpTrackNum() const { return m_rtpTrackNum; }
    quint8 getRtcpTrackNum() const { return m_rtpTrackNum+1; }
private:
    void processRtcpData();
private:
    RTPSession* m_owner;
    bool m_tcpMode;
    RtspStatistic m_statistic;
    AbstractDatagramSocket* m_mediaSocket;
    AbstractDatagramSocket* m_rtcpSocket;
    quint32 ssrc;
    quint8 m_rtpTrackNum;
};

class RTPSession: public QObject
{
    Q_OBJECT;
public:
    // TODO: #Elric #enum
    enum DefaultAuthScheme {
        authNone,
        authBasic,
        authDigest // generate nonce as current time in usec
    };

    //typedef QMap<int, QScopedPointer<RTPIODevice> > RtpIoTracks;

    enum TrackType {TT_VIDEO, TT_VIDEO_RTCP, TT_AUDIO, TT_AUDIO_RTCP, TT_METADATA, TT_METADATA_RTCP, TT_UNKNOWN, TT_UNKNOWN2};
    enum TransportType {TRANSPORT_UDP, TRANSPORT_TCP, TRANSPORT_AUTO };

    struct SDPTrackInfo
    {
        SDPTrackInfo(const QString& _codecName, QString trackTypeStr, const QString& _setupURL, int _mapNum, int _trackNum, RTPSession* owner, bool useTCP):
            codecName(_codecName), setupURL(_setupURL), mapNum(_mapNum), trackNum(_trackNum)
        {
            trackTypeStr = trackTypeStr.toLower();
            if (trackTypeStr == QLatin1String("audio"))
                trackType = TT_AUDIO;
            else if (trackTypeStr == QLatin1String("audio-rtcp"))
                trackType = TT_AUDIO_RTCP;
            else if (trackTypeStr == QLatin1String("video"))
                trackType = TT_VIDEO;
            else if (trackTypeStr == QLatin1String("video-rtcp"))
                trackType = TT_VIDEO_RTCP;
            else if (trackTypeStr == QLatin1String("metadata"))
                trackType = TT_METADATA;
            else
                trackType = TT_UNKNOWN;

            ioDevice = new RTPIODevice(owner, useTCP);
            ioDevice->setRtpTrackNum(_trackNum * 2);
            interleaved = QPair<int,int>(-1,-1);
        }

        void setSSRC(quint32 value);
        quint32 getSSRC() const;

        ~SDPTrackInfo() { delete ioDevice; }

        QString codecName;
        TrackType trackType;
        QString setupURL;
        int mapNum;
        int trackNum;
        QPair<int,int> interleaved;

        RTPIODevice* ioDevice;
    };

    static QString mediaTypeToStr(TrackType tt);

    //typedef QMap<int, QSharedPointer<SDPTrackInfo> > TrackMap;
    typedef QVector<QSharedPointer<SDPTrackInfo> > TrackMap;

    RTPSession();
    ~RTPSession();

    // returns \a CameraDiagnostics::ErrorCode::noError if stream was opened, error code - otherwise
    CameraDiagnostics::Result open(const QString& url, qint64 startTime = AV_NOPTS_VALUE);

    /*
    * Start playing RTSP sessopn.
    * @param positionStart start position at mksec
    * @param positionEnd end position at mksec
    * @param scale playback speed
    */
    RTPSession::TrackMap play(qint64 positionStart, qint64 positionEnd, double scale);

    // returns true if there is no error delivering STOP
    bool stop();


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
    void setAuth(const QAuthenticator& auth, DefaultAuthScheme defaultAuthScheme);
    QAuthenticator getAuth() const;

    void setProxyAddr(const QString& addr, int port);

    /*
    * Find track by type ('video', 'audio' e t.c.) and returns track IO device. Returns 0 if track not found.
    */
    RTPIODevice* getTrackIoByType(TrackType trackType);

    QString getCodecNameByType(TrackType trackType);
    QList<QByteArray> getSdpByType(TrackType trackType) const;

    int getLastResponseCode() const;

    void setAudioEnabled(bool value);

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


    void sendBynaryResponse(quint8* buffer, int size);

    RtspStatistic parseServerRTCPReport(quint8* srcBuffer, int srcBufferSize, bool* gotStatistics);
    int buildClientRTCPReport(quint8 *dstBuffer, int bufferLen);

    void setUsePredefinedTracks(int numOfVideoChannel);

    static quint8* prepareDemuxedData(std::vector<QnByteArray*>& demuxedData, int channel, int reserve);

    bool setTCPReadBufferSize(int value);

    QString getVideoLayout() const;
signals:
    void gotTextResponse(QByteArray text);
private:
    void addRangeHeader(QByteArray& request, qint64 startPos, qint64 endPos);
    QString getTrackFormat(int trackNum) const;
    TrackType getTrackType(int trackNum) const;
    //int readRAWData();
    QByteArray createDescribeRequest();
    bool sendDescribe();
    bool sendOptions();
    bool sendSetup();
    bool sendKeepAlive();

    bool readTextResponce(QByteArray &responce);
    void addAuth(QByteArray& request);


    QString extractRTSPParam(const QString &buffer, const QString &paramName);
    void updateTransportHeader(QByteArray &responce);

    void parseSDP();
    void updateTrackNum();
    void addAdditionAttrs(QByteArray& request);
    void updateResponseStatus(const QByteArray& response);

    // in case of error return false
    bool isDigestAuthRequired(const QByteArray& response);
    void usePredefinedTracks();
    bool processTextResponseInsideBinData();
    static QByteArray getGuid();
    void registerRTPChannel(int rtpNum, QSharedPointer<SDPTrackInfo> trackInfo);
    QByteArray calcDefaultNonce() const;
    QByteArray createPlayRequest( qint64 startPos, qint64 endPos );
    bool sendPlayInternal(qint64 startPos, qint64 endPos);
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
    int m_proxyPort;
    int m_responseCode;
    bool m_isAudioEnabled;
    bool m_useDigestAuth;
    int m_numOfPredefinedChannels;
    unsigned int m_TimeOut;
    // end of initialized fields

    //unsigned char m_responseBuffer[MAX_RESPONCE_LEN];
    quint8* m_responseBuffer;
    int m_responseBufferLen;
    QByteArray m_sdp;

    std::unique_ptr<AbstractStreamSocket> m_tcpSock;
    //RtpIoTracks m_rtpIoTracks; // key: tracknum, value: track IO device

    QUrl mUrl;

    QString m_SessionId;
    unsigned short m_ServerPort;
    // format: key - track number, value - codec name
    TrackMap m_sdpTracks;

    QElapsedTimer m_keepAliveTime;

    friend class RTPIODevice;
    QMap<QByteArray, QByteArray> m_additionAttrs;
    QAuthenticator m_auth;
    QString m_proxyAddr;
    QString m_contentBase;
    TransportType m_prefferedTransport;

    QString m_realm;
    QString m_nonce;

    static QByteArray m_guid; // client guid. used in proprietary extension
    static QMutex m_guidMutex;

    QVector<QSharedPointer<SDPTrackInfo> > m_rtpToTrack;
    QString m_reasonPhrase;
    QString m_videoLayout;

    char* m_additionalReadBuffer;
    int m_additionalReadBufferPos;
    int m_additionalReadBufferSize;

    /*!
        \param readSome if \a true, returns as soon as some data has been read. Otherwise, blocks till all \a bufSize bytes has been read
    */
    int readSocketWithBuffering( quint8* buf, size_t bufSize, bool readSome );

    //!Sends request and waits for response. If request requires authentication, re-tries request with credentials
    /*!
        Updates \a m_responseCode member.
        \return error description
    */
    bool sendRequestAndReceiveResponse( const QByteArray& request, QByteArray& responce );
};

#endif //rtp_session_h_1935_h
