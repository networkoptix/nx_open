#ifndef rtp_session_h_1935_h
#define rtp_session_h_1935_h

#include "socket.h"

#include <QtCore/QDateTime>
#include <QtCore/QUrl>

class RTPSession;

static const int RTSP_FFMPEG_VIDEO_HEADER_SIZE = 5;

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

class RTPSession
{
public:
    struct SDPTrackInfo
    {
        SDPTrackInfo() {}
        SDPTrackInfo(const QString& _codecName, const QString& _codecType): codecName(_codecName), codecType(_codecType) {}
        QString codecName;
        QString codecType;
    };

    RTPSession();
    ~RTPSession();

    // returns true if stream was opened, false in case of some error
    bool open(const QString& url);

    // position at mksec
    RTPIODevice* play(qint64 position = 0, double scale = 1.0);

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

    bool sendPlay(qint64 startPos, qint64 endPos, double scale);
    bool sendPause();

    int lastSendedCSeq() const { return m_csec-1; }
private:
    qint64 m_startTime;
    qint64 m_endTime;
    int readRAWData();
    bool sendDescribe();
    bool sendOptions();
    RTPIODevice *sendSetup();
    bool sendTeardown();
    bool sendKeepAlive();

    bool readTextResponce(QByteArray &responce);
    int readBinaryResponce(quint8 *data, int maxDataSize);


    QString extractRTSPParam(const QString &buffer, const QString &paramName);
    void updateTransportHeader(QByteArray &responce);

    void parseSDP();
    int buildRTCPReport(quint8 *dstBuffer, const RtspStatistic *stats);
    void parseRangeHeader(const QString& rangeStr);
private:
    enum { RTSP_BUFFER_LEN = 1024 * 64 * 10 };

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
};

#endif //rtp_session_h_1935_h
