#ifndef rtp_session_h_1935_h
#define rtp_session_h_1935_h

#include <QAuthenticator>
#include "socket.h"

#include <QtCore/QDateTime>
#include <QtCore/QUrl>

class RTPSession;

static const int RTSP_FFMPEG_GENERIC_HEADER_SIZE = 8;
static const int RTSP_FFMPEG_VIDEO_HEADER_SIZE = 3;
static const int RTSP_FFMPEG_METADATA_HEADER_SIZE = 4;
static const int RTSP_FFMPEG_MAX_HEADER_SIZE = RTSP_FFMPEG_GENERIC_HEADER_SIZE + RTSP_FFMPEG_METADATA_HEADER_SIZE;
static const int MAX_RTP_PACKET_SIZE = 1024 * 16;

class RtspStatistic 
{
public:
    RtspStatistic(): timestamp(0), nptTime(0), receivedPackets(0), receivedOctets(0) {}
    bool isEmpty() const { return timestamp == 0 && nptTime == 0; }

    quint32 timestamp;
    double nptTime;
    qint64 receivedPackets;
    qint64 receivedOctets;
};

class QnRtspTimeHelper
{
public:
    QnRtspTimeHelper();

    qint64 getUsecTime(quint32 rtpTime, const RtspStatistic& statistics, int rtpFrequency, bool recursiveAllowed = true);
    void reset();
private:
    double cameraTimeToLocalTime(double cameraTime); // time in seconds since 1.1.1970
private:
    double m_cameraClockToLocalDiff; // in secs
};

class RTPIODevice
{
public:
    explicit RTPIODevice(RTPSession* owner, bool useTCP);
    virtual ~RTPIODevice();
    virtual qint64 read(char * data, qint64 maxSize );
    
    const RtspStatistic& getStatistic() { return m_statistic;}
    void setStatistic(const RtspStatistic& value) { m_statistic = value; }
    CommunicatingSocket* getMediaSocket();
    UDPSocket* getRtcpSocket() const { return m_rtcpSocket; }
    void setTcpMode(bool value);
private:
    void processRtcpData();
private:
    RTPSession* m_owner;
    bool m_tcpMode;
    RtspStatistic m_statistic;
    UDPSocket* m_mediaSocket;
    UDPSocket* m_rtcpSocket;
};

class RTPSession: public QObject
{
    Q_OBJECT;
public:

    //typedef QMap<int, QScopedPointer<RTPIODevice> > RtpIoTracks;

    enum TrackType {TT_VIDEO, TT_VIDEO_RTCP, TT_AUDIO, TT_AUDIO_RTCP, TT_METADATA, TT_METADATA_RTCP, TT_UNKNOWN};

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
        }
        ~SDPTrackInfo() { delete ioDevice; }

        QString codecName;
        TrackType trackType;
        QString setupURL;
        int mapNum;
        int trackNum;

        RTPIODevice* ioDevice;
    };

    static QString mediaTypeToStr(TrackType tt);

    //typedef QMap<int, QSharedPointer<SDPTrackInfo> > TrackMap;
    typedef QList<QSharedPointer<SDPTrackInfo> > TrackMap;

    RTPSession();
    ~RTPSession();

    // returns true if stream was opened, false in case of some error
    bool open(const QString& url);

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

    void setTransport(const QString& transport);
    QString getTransport() const;
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

    void setAuth(const QAuthenticator& auth);
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

    int readBinaryResponce(quint8 *data, int maxDataSize);
    void sendBynaryResponse(quint8* buffer, int size);

    RtspStatistic parseServerRTCPReport(quint8* srcBuffer, int srcBufferSize);
    int buildClientRTCPReport(quint8 *dstBuffer);

    void setSimpleOpenMode();
signals:
    void gotTextResponse(QByteArray text);
private:
    QString getTrackFormat(int trackNum) const;
    TrackType getTrackType(int trackNum) const;
    int readRAWData();
    bool sendDescribe();
    bool sendOptions();
    bool sendSetup();
    bool sendKeepAlive();

    bool readTextResponce(QByteArray &responce);
    void addAuth(QByteArray& request);


    QString extractRTSPParam(const QString &buffer, const QString &paramName);
    void updateTransportHeader(QByteArray &responce);

    void parseSDP();
    void addAdditionAttrs(QByteArray& request);
    void updateResponseStatus(const QByteArray& response);

    // in case of error return false
    bool checkIfDigestAuthIsneeded(const QByteArray& response);
    void usePredefinedTracks();
private:
    enum { RTSP_BUFFER_LEN = 1024 * 64 * 16 };

    // 'initialization in order' block
    unsigned int m_csec;
    QString m_transport;
    int m_selectedAudioChannel;
    qint64 m_startTime;
    qint64 m_endTime;
    float m_scale;
    int m_tcpTimeout;
    int m_proxyPort;
    int m_responseCode;
    bool m_isAudioEnabled;
    bool m_useDigestAuth;
    // end of initialized fields

    //unsigned char m_responseBuffer[MAX_RESPONCE_LEN];
    quint8* m_responseBuffer;
    int m_responseBufferLen;
    QByteArray m_sdp;

    TCPSocket m_tcpSock;
    //RtpIoTracks m_rtpIoTracks; // key: tracknum, value: track IO device

    QUrl mUrl;

    QString m_SessionId;
    unsigned short m_ServerPort;
    // format: key - track number, value - codec name
    TrackMap m_sdpTracks;

    unsigned int m_TimeOut;

    QTime m_keepAliveTime;

    friend class RTPIODevice;
    QMap<QByteArray, QByteArray> m_additionAttrs;
    QAuthenticator m_auth;
    QString m_proxyAddr;
    QString m_contentBase;
    QString m_prefferedTransport;

    QString m_realm;
    QString m_nonce;
    bool m_useSimpleOpen;
};

#endif //rtp_session_h_1935_h
