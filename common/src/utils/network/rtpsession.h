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

struct RtspStatistic {
    quint32 timestamp;
    double nptTime;
    qint64 receivedPackets;
    qint64 receivedOctets;
};

class RTPIODevice
{
public:
    explicit RTPIODevice(RTPSession& owner);
    virtual ~RTPIODevice();
    virtual qint64    read(char * data, qint64 maxSize );
    void setTCPMode(bool mode) {m_tcpMode = mode;}
    void setSocket(CommunicatingSocket* socket) { m_sock = socket; }
    CommunicatingSocket* getSocket() const { return m_sock; }
    
private:
    CommunicatingSocket* m_sock;
    RTPSession& m_owner;
    qint64 m_receivedPackets;
    qint64 m_receivedOctets;
    bool m_tcpMode;
};

class RTPSession: public QObject
{
    Q_OBJECT;
public:

    struct SDPTrackInfo
    {
        SDPTrackInfo() {}
        SDPTrackInfo(const QString& _codecName, const QString& _codecType, const QString& _setupURL): codecName(_codecName), codecType(_codecType), setupURL(_setupURL) {}
        QString codecName;
        QString codecType;
        QString setupURL;
    };

    RTPSession();
    ~RTPSession();

    // returns true if stream was opened, false in case of some error
    bool open(const QString& url);

    // position at mksec
    RTPIODevice* play(qint64 positionStart, qint64 positionEnd, double scale);

    // returns true if there is no error delivering STOP
    bool stop();


    // returns true if session is opened
    bool isOpened() const;

    // session timeout in ms
    unsigned int sessionTimeoutMs();

    const QByteArray& getSdp() const;

    bool sendKeepAliveIfNeeded(const RtspStatistic *stats);
    void processRtcpData(const RtspStatistic *stats);

    void setTransport(const QString& transport);
    QString getTrackFormat(int trackNum) const;
    QString getTrackType(int trackNum) const;

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

signals:
    void gotTextResponse(QByteArray text);
private:
    qint64 m_startTime;
    qint64 m_endTime;
    int readRAWData();
    bool sendDescribe();
    bool sendOptions();
    RTPIODevice *sendSetup();
    bool sendKeepAlive();

    bool readTextResponce(QByteArray &responce);
    int readBinaryResponce(quint8 *data, int maxDataSize);
    void addAuth(QByteArray& request);


    QString extractRTSPParam(const QString &buffer, const QString &paramName);
    void updateTransportHeader(QByteArray &responce);

    void parseSDP();
    int buildRTCPReport(quint8 *dstBuffer, const RtspStatistic *stats);
    void addAdditionAttrs(QByteArray& request);

private:
    enum { RTSP_BUFFER_LEN = 1024 * 64 * 16 };

    //unsigned char m_responseBuffer[MAX_RESPONCE_LEN];
    quint8* m_responseBuffer;
    int m_responseBufferLen;
    QByteArray m_sdp;

    TCPSocket m_tcpSock;
    UDPSocket m_udpSock;
    UDPSocket m_rtcpUdpSock;
    RTPIODevice m_rtpIo;
    int m_selectedAudioChannel;

    QUrl mUrl;

    unsigned int m_csec;
    QString m_SessionId;
    unsigned short m_ServerPort;
    // format: key - track number, value - codec name
    QMap<int, SDPTrackInfo> m_sdpTracks;

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
};

#endif //rtp_session_h_1935_h
