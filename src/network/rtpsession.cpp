#include "rtpsession.h"
#include "base/sleep.h"

#define DEFAULT_RTP_PORT 554
#define RESERVED_TIMEOUT_TIME (5*1000)


RTPIODevice::RTPIODevice(RTPSession& owner, CommunicatingSocket& sock)
:m_sock(sock),
m_owner(owner)
{

}

RTPIODevice::~RTPIODevice()
{

}


qint64	RTPIODevice::read( char * data, qint64 maxSize )
{
    m_owner.sendKeepAliveIfNeeded();
    int readed = m_sock.recv(data, maxSize);
    return readed;
}



//============================================================================================
RTPSession::RTPSession():
m_csec(2),
m_udpSock(0),
m_rtpIo(*this, m_udpSock)
{
    m_udpSock.setTimeOut(500);
    m_tcpSock.setTimeOut(3*1000);
}

RTPSession::~RTPSession()
{

}

void RTPSession::parseSDP()
{
    QList<QByteArray> lines = m_sdp.split('\n');

    int mapNum = -1; 
    QString codecName;
    foreach(QByteArray line, lines)
    {
        line = line.trimmed().toLower();
        if (line.startsWith("a=rtpmap"))
        {
            QList<QByteArray> params = line.split(' ');
            if (params.size() < 2)
                continue; // invalid data format. skip
            QList<QByteArray> trackInfo = params[0].split(':');
            QList<QByteArray> codecInfo = params[1].split('/');
            if (trackInfo.size() < 2 || codecInfo.size() < 2)
                continue; // invalid data format. skip
            mapNum = trackInfo[1].toUInt();
            codecName = codecInfo[0];
        }
        else if (line.startsWith("a=control:trackid="))
        {
            int trackNum = line.mid(QString("a=control:trackid=").length()).toUInt();
            m_sdpTracks.insert(codecName, trackNum);
        }
    }
}


bool RTPSession::open(const QString& url)
{
    mUrl = url; 

    unsigned int port = DEFAULT_RTP_PORT;

    if (!m_tcpSock.connect(mUrl.host().toLatin1().data(), DEFAULT_RTP_PORT))
        return false;

    if (!sendDescribe())
        return false;

    QByteArray sdp;

    if (!readResponce(sdp))
        return false;

    int sdp_index = sdp.indexOf("\r\n\r\n");

    if (sdp_index  < 0 || sdp_index+4 >= sdp.size())
        return false;

    m_sdp = sdp.mid(sdp_index+4);
    parseSDP();

    if (m_sdpTracks.size()<=0)
        return false;

    return true;
}

RTPIODevice* RTPSession::play()
{
    RTPIODevice* ioDevice = sendSetup();

    if (ioDevice == 0)
        return 0;

    
    if (!sendPlay())
        return 0;


    return &m_rtpIo;

}

bool RTPSession::stop()
{

    return true;
}


bool RTPSession::isOpened() const
{
    return m_tcpSock.isConnected();
}

unsigned int RTPSession::sessionTimeoutMs()
{
    return 0;
}

const QByteArray& RTPSession::getSdp() const
{
    return m_sdp;
}

//===================================================================

bool RTPSession::sendDescribe()
{
    QByteArray request;
    request += "DESCRIBE ";
    request += mUrl.toString();
    request += " RTSP/1.0\r\n";
    request += "CSeq: ";
    request += QByteArray::number(m_csec++);
    request += "\r\n";
    request += "User-Agent: Network Optix\r\n";
    request += "Accept: application/sdp\r\n\r\n";
    
    //qDebug() << request;

    if (!m_tcpSock.send(request.data(), request.size()))
        false;

    return true;

}

bool RTPSession::sendOptions()
{
    QByteArray request;
    request += "OPTIONS ";
    request += mUrl.toString();
    request += " RTSP/1.0\r\n";
    request += "CSeq: ";
    request += QByteArray::number(m_csec++);
    request += "\r\n\r\n";


    if (!m_tcpSock.send(request.data(), request.size()))
        false;

    return true;

}

RTPIODevice*  RTPSession::sendSetup()
{
    QByteArray request;
    request += "SETUP ";
    request += mUrl.toString();

    if (!m_sdpTracks.isEmpty())
        request += QString("/trackID=") + QString::number(m_sdpTracks.begin().value());

    request += " RTSP/1.0\r\n";
    request += "CSeq: ";
    request += QByteArray::number(m_csec++);
    request += "\r\n";
    request += "User-Agent: Network Optix\r\n";
    request += "Transport: RTP/AVP;unicast;client_port=";
    

    request += QString::number(m_udpSock.getLocalPort());
    request += "\r\n\r\n";

    //qDebug() << request;

    if (!m_tcpSock.send(request.data(), request.size()))
        return 0;

    QByteArray responce;

    if (!readResponce(responce))
        return 0;


    if (!responce.startsWith("RTSP/1.0 200")) 
    {
        return 0;
    }


    m_TimeOut = 0; // default timeout 0 ( do not send keep alive )

    QString tmp = extractRTSPParam(responce, "Session:");

    if (tmp.size() > 0) 
    {
        QStringList tmpList = tmp.split(';');
        m_SessionId = tmpList[0];

        for (int i = 0; i < tmpList.size(); ++i)
        {
            if (tmpList[i].startsWith("timeout"))
            {
                QStringList tmpParams = tmpList[i].split('=');
                if (tmpParams.size() > 1)
                    m_TimeOut = tmpParams[1].toInt();
            }
        }
    }

    updateTransportHeader(responce);

    return &m_rtpIo;

}

bool RTPSession::sendPlay()
{

    QByteArray request;
    QByteArray responce;

    request += "PLAY ";
    request += mUrl.toString();
    request += " RTSP/1.0\r\n";
    request += "CSeq: ";
    request += QByteArray::number(m_csec++);
    request += "\r\n";
    request += "Session: ";
    request += m_SessionId;
    request += "\r\n";
    request += "Range: npt=0.000"; // offset
    request += "-\r\n\r\n";

    if (!m_tcpSock.send(request.data(), request.size()))
        return false;


    if (!readResponce(responce))
        return false;


    if (responce.startsWith("RTSP/1.0 200"))
    {
        updateTransportHeader(responce);
        m_keepAliveTime.restart();
        return true;
    }

    return false;

}

bool RTPSession::sendTeardown()
{
    QByteArray request;
    QByteArray responce;
    request += "TEARDOWN ";
    request += mUrl.toString();
    request += " RTSP/1.0\r\n";
    request += "CSeq: ";
    request += QByteArray::number(m_csec++);
    request += "\r\n";
    request += "Session: ";
    request += m_SessionId;
    request += "\r\n\r\n";

    if (!m_tcpSock.send(request.data(), request.size()))
        false;


    
    if (!readResponce(responce) || !responce.startsWith("RTSP/1.0 200"))
    {
        return false;
    }
    else 
    {
        //d->lastSendTime.start();
        return 0;
    }

}

bool RTPSession::sendKeepAliveIfNeeded()
{
    if (m_TimeOut==0)
        return true;

    if (m_keepAliveTime.elapsed() < m_TimeOut - RESERVED_TIMEOUT_TIME)
        return true;
    else 
    {
        bool res= sendKeepAlive();
        m_keepAliveTime.restart();
        return res;
    }
}

bool RTPSession::sendKeepAlive()
{

    QByteArray request;
    QByteArray responce;
    request += "GET_PARAMETER ";
    request += mUrl.toString();
    request += " RTSP/1.0\r\n";
    request += "CSeq: ";
    request += QByteArray::number(m_csec++);
    //if (!d->sessionId.isEmpty()) 
    {
        request += "\r\n";
        request += "Session: ";
        request += m_SessionId;
    }
    request += "\r\n\r\n";
    //


    if (!m_tcpSock.send(request.data(), request.size()))
        false;


    if(!readResponce(responce) || !responce.startsWith("RTSP/1.0 200"))
    {
        return false;
    }
    else 
    {
        return true;
    }
}




bool RTPSession::readResponce(QByteArray& responce)
{

    int readed = m_tcpSock.recv(mResponse, sizeof(mResponse));

    if (readed<=0)
        return false;

    responce = QByteArray::fromRawData((char*)mResponse, readed);
    return true;
}

QString RTPSession::extractRTSPParam(const QString& buffer, const QString& paramName)
{
    int pos = buffer.indexOf(paramName);
    if (pos > 0)
    {
        QString rez;
        bool first = true;
        for (int i = pos + paramName.size() + 1; i < buffer.size(); i++)
        {
            if (buffer[i] == ' ' && first)
                continue;
            else if (buffer[i] == '\r' || buffer[i] == '\n')
                break;
            else {
                rez += buffer[i];
                first = false;
            }
        }
        return rez;
    }
    else
        return "";
}

void RTPSession::updateTransportHeader(QByteArray& responce)
{
    QString tmp = extractRTSPParam(responce, "Transport:");
    if (tmp.size() > 0) 
    {
        QStringList tmpList = tmp.split(';');
        for (int i = 0; i < tmpList.size(); ++i)
        {
            if (tmpList[i].startsWith("port"))
            {
                QStringList tmpParams = tmpList[i].split('=');
                if (tmpParams.size() > 1)
                    m_ServerPort = tmpParams[1].toInt();
            }
        }
    }
}
