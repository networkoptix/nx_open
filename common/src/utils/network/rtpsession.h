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
    CommunicatingSocket* getMediaSocket();
    UDPSocket* getRtcpSocket() const { return m_rtcpSocket; }
private:
    void processRtcpData();
private:
    UDPSocket* m_mediaSocket;
    UDPSocket* m_rtcpSocket;

    RTPSession* m_owner;
    bool m_tcpMode;
    RtspStatistic m_statistic;
};

class RTPSession: public QObject
{
    Q_OBJECT;
public:

    //typedef QMap<int, QScopedPointer<RTPIODevice> > RtpIoTracks;

    struct SDPTrackInfo
    {
        SDPTrackInfo(const QString& _codecName, const QString& _codecType, const QString& _setupURL, int _mapNum, int _trackNum, RTPSession* owner, bool useTCP):
            codecName(_codecName), codecType(_codecType), setupURL(_setupURL), mapNum(_mapNum), trackNum(_trackNum)
        {
            ioDevice = new RTPIODevice(owner, useTCP);
        }
        ~SDPTrackInfo() { delete ioDevice; }

        QString codecName;
        QString codecType;
        QString setupURL;
        int mapNum;
        int trackNum;

        RTPIODevice* ioDevice;
    };

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
    QString getTrackFormatByRtpChannelNum(int channelNum);

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
    RTPIODevice* getTrackIoByType(const QString& trackType);

    QString getCodecNameByType(const QString& trackType);
    QList<QByteArray> getSdpByType(const QString& trackType) const;

    int getLastResponseCode() const;
signals:
    void gotTextResponse(QByteArray text);
private:
    qint64 m_startTime;
    qint64 m_endTime;

    QString getTrackFormat(int trackNum) const;
    int readRAWData();
    bool sendDescribe();
    bool sendOptions();
    bool sendSetup();
    bool sendKeepAlive();

    bool readTextResponce(QByteArray &responce);
    int readBinaryResponce(quint8 *data, int maxDataSize);
    void addAuth(QByteArray& request);


    QString extractRTSPParam(const QString &buffer, const QString &paramName);
    void updateTransportHeader(QByteArray &responce);

    void parseSDP();
    RtspStatistic parseServerRTCPReport(quint8* srcBuffer, int srcBufferSize);
    int buildClientRTCPReport(quint8 *dstBuffer);
    void addAdditionAttrs(QByteArray& request);
    void updateResponseStatus(const QByteArray& response);
private:
    enum { RTSP_BUFFER_LEN = 1024 * 64 * 16 };

    //unsigned char m_responseBuffer[MAX_RESPONCE_LEN];
    quint8* m_responseBuffer;
    int m_responseBufferLen;
    QByteArray m_sdp;

    TCPSocket m_tcpSock;
    //RtpIoTracks m_rtpIoTracks; // key: tracknum, value: track IO device
    int m_selectedAudioChannel;

    QUrl mUrl;

    unsigned int m_csec;
    QString m_SessionId;
    unsigned short m_ServerPort;
    // format: key - track number, value - codec name
    TrackMap m_sdpTracks;

    unsigned int m_TimeOut;

    QTime m_keepAliveTime;
    QString m_transport;
    float m_scale;

    friend class RTPIODevice;
    QMap<QByteArray, QByteArray> m_additionAttrs;
    int m_tcpTimeout;
    QAuthenticator m_auth;
    QString m_proxyAddr;
    int m_proxyPort;
    QString m_contentBase;
    int m_responseCode;
};

#endif //rtp_session_h_1935_h
