#ifndef rtp_session_h_1935_h
#define rtp_session_h_1935_h
#include "socket.h"

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
    explicit RTPIODevice(RTPSession& owner,  CommunicatingSocket& sock);
    virtual ~RTPIODevice();
    virtual qint64	read(char * data, qint64 maxSize );


private:

    CommunicatingSocket& m_sock;
    RTPSession& m_owner;
    qint64 m_receivedPackets;
    qint64 m_receivedOctets;
};

class RTPSession
{
public:
    RTPSession();
    ~RTPSession();

    // returns true if stream was opened, false in case of some error
    bool open(const QString& url);

    RTPIODevice* play();

	// returns true if there is no error delivering STOP
	bool stop();


    // returns true if session is opened
    bool isOpened() const;

	// session timeout in ms
	unsigned int sessionTimeoutMs();

	const QByteArray& getSdp() const;

    bool sendKeepAliveIfNeeded(const RtspStatistic* stats);
    void processRtcpData(const RtspStatistic* stats);
private:

    bool sendDescribe();
    bool sendOptions();
    RTPIODevice*  sendSetup();
    bool sendPlay();
    bool sendTeardown();
    bool sendKeepAlive();

    bool readResponse(QByteArray &response);


    QString extractRTSPParam(const QByteArray& buffer, const QByteArray& paramName);
    void updateTransportHeader(QByteArray &response);

    void parseSDP();
    int buildRTCPReport(quint8* dstBuffer, const RtspStatistic* stats);
private:

    enum {MAX_RESPONCE_LEN	= 1024*8};

    unsigned char mResponse[MAX_RESPONCE_LEN];
    QByteArray m_sdp;

    TCPSocket m_tcpSock;
    UDPSocket m_udpSock;
    UDPSocket m_rtcpUdpSock;
    RTPIODevice m_rtpIo;


    QUrl mUrl;

    unsigned int m_csec;
    QString m_SessionId;
    unsigned short m_ServerPort;
    // format: key - codc name, value - track number
    QMap<QString, int> m_sdpTracks;

    unsigned int m_TimeOut;

    QTime m_keepAliveTime;
};

#endif //rtp_session_h_1935_h
