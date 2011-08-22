#ifndef rtp_session_h_1935_h
#define rtp_session_h_1935_h

#include "socket.h"

#include <QtCore/QDateTime>
#include <QtCore/QUrl>

class RTPSession;

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
    const CommunicatingSocket* getSocket() const { return m_sock; }
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

private:
    int RTPSession::readRAWData();
    bool sendDescribe();
    bool sendOptions();
    RTPIODevice *sendSetup();
    bool sendPlay(qint64 position, double scale);
    bool sendTeardown();
    bool sendKeepAlive();

    bool readTextResponce(QByteArray &responce);
    int readBinaryResponce(quint8 *data, int maxDataSize);


    QString extractRTSPParam(const QString &buffer, const QString &paramName);
    void updateTransportHeader(QByteArray &responce);

    void parseSDP();
    int buildRTCPReport(quint8 *dstBuffer, const RtspStatistic *stats);

private:
    enum { MAX_RESPONCE_LEN = 1024*70 };

    //unsigned char m_responseBuffer[MAX_RESPONCE_LEN];
    quint8* m_responseBuffer;
    int m_responseBufferLen;
    QByteArray m_sdp;

    TCPSocket* m_tcpSock;
    UDPSocket m_udpSock;
    UDPSocket m_rtcpUdpSock;
    RTPIODevice m_rtpIo;


    QUrl mUrl;

    unsigned int m_csec;
    QString m_SessionId;
    unsigned short m_ServerPort;
    // format: key - track number, value - codec name
    QMap<int, QString> m_sdpTracks;

    unsigned int m_TimeOut;

    QTime m_keepAliveTime;

    QString m_transport;

    friend class RTPIODevice;
};

#endif //rtp_session_h_1935_h
