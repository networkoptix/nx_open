#ifndef rtp_session_h_1935_h
#define rtp_session_h_1935_h
#include "socket.h"

class RTPSession;

class RTPIODevice 
{
public:
    explicit RTPIODevice(RTPSession& owner,  CommunicatingSocket& sock);
    virtual ~RTPIODevice();
    virtual qint64	read(char * data, qint64 maxSize );
    
    
private:

    CommunicatingSocket& m_sock;
    RTPSession& m_owner;

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

    bool sendKeepAliveIfNeeded();

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
private:

    enum {MAX_RESPONCE_LEN	= 1024*8};

    unsigned char mResponse[MAX_RESPONCE_LEN];
    QByteArray m_sdp;

    TCPSocket m_tcpSock;
    UDPSocket m_udpSock;
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