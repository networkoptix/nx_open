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

    void setTransport(const QString& transport);
    QString getTrackFormat(int trackNum) const;
private:

    bool sendDescribe();
    bool sendOptions();
    RTPIODevice*  sendSetup();
    bool sendPlay();
    bool sendTeardown();
    bool sendKeepAlive();

    bool readResponce(QByteArray& responce);


    QString extractRTSPParam(const QString& buffer, const QString& paramName);
    void updateTransportHeader(QByteArray& responce);

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
    QMap<int, QString> m_sdpTracks;
    unsigned int m_TimeOut;
    QTime m_keepAliveTime;
   
    QString m_transport;
};

#endif //rtp_session_h_1935_h