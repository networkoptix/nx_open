#ifndef rtp_session_h_1935_h
#define rtp_session_h_1935_h
#include "socket.h"

class RTPIODevice : public QIODevice
{
public:
    explicit RTPIODevice(CommunicatingSocket& sock);
    virtual ~RTPIODevice();

private:
    virtual qint64	readData ( char * data, qint64 maxSize );
    virtual qint64	writeData ( const char * data, qint64 maxSize );
private:

    CommunicatingSocket& m_sock;

};

class RTPSession
{
public:
    RTPSession();
    ~RTPSession();

    // returns true if stream was opened, false in case of some error
    bool open(const QString& url);
    
	QIODevice* play();

    // returns true if there is no error delivering STOP
	bool stop();


    // returns true if session is opened 
    bool isOpened() const;

    // session timeout in ms
	unsigned int sessionTimeoutMs();

    const QByteArray& getSdp() const;


private:

    bool sendDescribe();
    bool sendOptions();
    QIODevice*  sendSetup();
    bool sendPlay();
    bool sendTeardown();

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

    unsigned int mcsec;
    QString mSessionId;
    unsigned short mServerPort;
    // format: key - codc name, value - track number
    QMap<QString, int> m_sdpTracks;

    unsigned int mTimeOut;
};


#endif //rtp_session_h_1935_h