#include <QSet>

#include "rtsp_connection.h"
#include "resourcecontrol/resource_pool.h"
#include "resource/media_resource.h"
#include "resource/media_resource_layout.h"
#include "dataconsumer/dataconsumer.h"
#include "network/rtp_stream_parser.h"
#include "ffmpeg/ffmpeg_helper.h"

static const int CODE_OK = 200;
static const int CODE_NOT_FOUND = 404;
static const int CODE_NOT_IMPLEMETED = 501;
static const int CODE_INTERNAL_ERROR = 500;


static const QString ENDL("\r\n");
static const quint8 RTP_FFMPEG_GENERIC_CODE = 102;
static const quint8 RTP_FFMPEG_CONTEXT_CODE = 103;
//static const QString RTP_FFMPEG_GENERIC_STR("FFMPEG");
static const QString RTP_FFMPEG_GENERIC_STR("mpeg4-generic"); // this line for debugging purpose with VLC player
static const int MAX_QUEUE_SIZE = 15;
static const int MAX_RTSP_DATA_LEN = 65535 - 4 - RtpHeader::RTP_HEADER_SIZE;
static const int CLOCK_FREQUENCY = 1000;
static const quint32 BASIC_FFMPEG_SSRC = 20000;
static const int MAX_CONTEXTS_AT_VIDEO = 8; // max ammount of difference codecContext used for one video channel.

class QnRtspDataProcessor: public QnAbstractDataConsumer
{
public:
    QnRtspDataProcessor(QnRtspConnectionProcessor* owner):
      QnAbstractDataConsumer(MAX_QUEUE_SIZE),
      m_owner(owner)
    {
        memset(m_sequence, 0, sizeof(m_sequence));
        m_timer.start();
    }
protected:
    void buildRtspTcpHeader(quint8 channelNum, quint32 ssrc, quint16 len, int markerBit)
    {
        m_rtspTcpHeader[0] = '$';
        m_rtspTcpHeader[1] = channelNum;
        quint16* lenPtr = (quint16*) &m_rtspTcpHeader[2];
        *lenPtr = htons(len+sizeof(RtpHeader));
        RtpHeader* rtp = (RtpHeader*) &m_rtspTcpHeader[4];
        rtp->version = RtpHeader::RTP_VERSION;
        rtp->padding = 0;  
        rtp->extension = 0; 
        rtp->CSRCCount = 0;  
        rtp->marker  =  markerBit; 
        rtp->payloadType = RTP_FFMPEG_GENERIC_CODE;
        rtp->sequence = htons(m_sequence[channelNum]++);
        rtp->timestamp = htonl(m_timer.elapsed()); 
        rtp->ssrc = htonl(ssrc); // source ID
    }

    virtual void processData(QnAbstractDataPacketPtr data)
    {
        QnAbstractMediaDataPacketPtr media = qSharedPointerDynamicCast<QnAbstractMediaDataPacket>(data);
        int rtspChannelNum = media->channelNumber;
        if (media->dataType == QnAbstractMediaDataPacket::AUDIO)
            rtspChannelNum += m_owner->numOfVideoChannels();
        AVCodecContext* ctx = (AVCodecContext*) media->context;
        if (!ctx)
            return;
        // one video channel may has several subchannels (video combined with frames from difference codecContext)
        // max amount of subchannels is MAX_CONTEXTS_AT_VIDEO. Each channel used 2 ssrc: for data and for CodecContext
        int subChannelNumber = m_ctxSended.indexOf(ctx);
        quint32 ssrc = BASIC_FFMPEG_SSRC + rtspChannelNum * MAX_CONTEXTS_AT_VIDEO*2;
        // serialize and send FFMPEG context to stream
        if (subChannelNumber == -1)
        {
            m_ctxSended << ctx;
            subChannelNumber = m_ctxSended.size();
            ssrc += subChannelNumber;
            QnFfmpegHelper::serializeCodecContext(ctx, &m_codecCtxData);
            buildRtspTcpHeader(rtspChannelNum, ssrc + 1, m_codecCtxData.size(), true); // ssrc+1 - switch data subchannel to context subchannel
            m_owner->sendData(m_rtspTcpHeader, sizeof(m_rtspTcpHeader));
            m_owner->sendData(m_codecCtxData.data());
        }
        else {
            ssrc += subChannelNumber;
        }
        // send data with RTP headers
        const char* curData = media->data.data();
        int sendLen = 0;
        for (int dataRest = media->data.size(); dataRest > 0; dataRest -= sendLen)
        {
            sendLen = qMin(MAX_RTSP_DATA_LEN, dataRest);
            buildRtspTcpHeader(rtspChannelNum, ssrc, sendLen, sendLen == dataRest);
            m_owner->sendData(m_rtspTcpHeader, sizeof(m_rtspTcpHeader));
            m_owner->sendData(curData, sendLen);
            curData += sendLen;
        }
    }
private:
    QByteArray m_codecCtxData;
    QList<AVCodecContext*> m_ctxSended;
    QTime m_timer;
    quint16 m_sequence[256];
    QnRtspConnectionProcessor* m_owner;
    char m_rtspTcpHeader[4 + RtpHeader::RTP_HEADER_SIZE];
};

// ----------------------------- QnRtspConnectionProcessor ----------------------------

class QnRtspConnectionProcessor::QnRtspConnectionProcessorPrivate
{
public:
    QnRtspConnectionProcessorPrivate():
        dataProvider(0),
        dataProcessor(0)
    {

    }
    QTcpSocket* socket;

    QHttpRequestHeader requestHeaders;
    QHttpResponseHeader responseHeaders;

    QByteArray protocol;
    QByteArray requestBody;
    QByteArray responseBody;
    QByteArray clientRequest;

    QString sessionId;
    QnAbstractMediaStreamDataProvider* dataProvider;
    QnAbstractDataConsumer* dataProcessor;
    QnMediaResourcePtr mediaRes; 
    // associate trackID with RTP/RTCP ports (for TCP mode ports used as logical channel numbers, see RFC 2326)
    QMap<int, QPair<int,int> > trackPorts; 
};

void QnRtspConnectionProcessor::sendData(const QByteArray& data)
{
    Q_D(QnRtspConnectionProcessor);
    d->socket->write(data);
}

void QnRtspConnectionProcessor::sendData(const char* data, int size)
{
    Q_D(QnRtspConnectionProcessor);
    d->socket->write(data);
}

QnRtspConnectionProcessor::QnRtspConnectionProcessor(QTcpSocket* socket):
    d_ptr(new QnRtspConnectionProcessorPrivate)
{
    Q_D(QnRtspConnectionProcessor);
    d->socket = socket;
    connect(socket, SIGNAL(connected()), this, SLOT(onClientConnected()));
    connect(socket, SIGNAL(connected()), this, SLOT(onClientDisconnected()));
    connect(socket, SIGNAL(readyRead()), this, SLOT(onClientReadyRead()));
}

QnRtspConnectionProcessor::~QnRtspConnectionProcessor()
{

}

void QnRtspConnectionProcessor::onClientConnected()
{

}

void QnRtspConnectionProcessor::onClientDisconnected()
{
    sender()->deleteLater();
}

bool QnRtspConnectionProcessor::isFullMessage()
{
    Q_D(QnRtspConnectionProcessor);
    QByteArray lRequest = d->clientRequest.toLower();
    QByteArray delimiter = "\n";
    int pos = lRequest.indexOf(delimiter);
    if (pos == -1)
        return false;
    if (pos > 0 && d->clientRequest[pos-1] == '\r')
        delimiter = "\r\n";
    int contentLen = 0;
    int contentLenPos = lRequest.indexOf("content-length") >= 0;
    if (contentLenPos)
    {
        int posStart = -1;
        int posEnd = -1;
        for (int i = contentLenPos+1; i < lRequest.length(); ++i)
        {
            if (lRequest[i] >= '0' && lRequest[i] <= '9')
            {
             if (posStart == -1)   
                 posStart = i;
             posEnd = i;
            }
            else if (lRequest[i] == '\n' || lRequest[i] == '\r')
                break;
        }
        if (posStart >= 0)
            contentLen = lRequest.mid(posStart, posEnd - posStart+1).toInt();
    }
    QByteArray dblDelim = delimiter + delimiter;
    return lRequest.endsWith(dblDelim) && (!contentLen || lRequest.indexOf(dblDelim) < lRequest.length()-dblDelim.length());
}

QString QnRtspConnectionProcessor::extractMediaName(const QString& path)
{
    int pos = path.indexOf("://");
    if (pos == -1)
        return QString();
    pos = path.indexOf('/', pos+3);
    if (pos == -1)
        return QString();
    return path.mid(pos+1);
}

void QnRtspConnectionProcessor::parseRequest()
{
    Q_D(QnRtspConnectionProcessor);
    QList<QByteArray> lines = d->clientRequest.split('\n');
    bool firstLine = true;
    foreach(const QByteArray& l, lines)
    {
        QByteArray line = l.trimmed();
        if (line.isEmpty())
            break;
        else  if (firstLine)
        {
            QList<QByteArray> params = line.split(' ');
            if (params.size() != 3)
            {
                qWarning() << Q_FUNC_INFO << "Invalid request format.";
                return;
            }
            QList<QByteArray> version = params[2].split('/');
            d->protocol = version[0];
            int major = 1;
            int minor = 0;
            if (version.size() > 1)
            {
                QList<QByteArray> v = version[1].split('.');
                major = v[0].toInt();
                if (v.length() > 1)
                    minor = v[1].toInt();
            }
            d->requestHeaders = QHttpRequestHeader(params[0], params[1], major, minor);
            firstLine = false;
        }
        else 
        {
            QList<QByteArray> params = line.split(':');
            if (params.size() > 1)
                d->requestHeaders.addValue(params[0], params[1]);
        }
    }
    QByteArray delimiter = "\n";
    int pos = d->clientRequest.indexOf(delimiter);
    if (pos == -1)
        return;
    if (pos > 0 && d->clientRequest[pos-1] == '\r')
        delimiter = "\r\n";
    QByteArray dblDelim = delimiter + delimiter;
    int bodyStart = d->clientRequest.indexOf(dblDelim);
    if (bodyStart >= 0 && d->requestHeaders.value("content-length").toInt() > 0)
        d->requestBody = d->clientRequest.mid(bodyStart + dblDelim.length());

    if (d->mediaRes == 0)
    {
        const QString resId = extractMediaName(d->requestHeaders.path());
        QnResourcePtr resource = QnResourcePool::instance().getResourceById(resId);
        if (resource == 0)
            resource = QnResourcePool::instance().getResourceByUrl(resId);
        d->mediaRes = qSharedPointerDynamicCast<QnMediaResource>(resource);
    }
    d->clientRequest.clear();
}

void QnRtspConnectionProcessor::initResponse(int code, const QString& message)
{
    Q_D(QnRtspConnectionProcessor);
    d->responseBody.clear();
    d->responseHeaders = QHttpResponseHeader(code, message, d->requestHeaders.majorVersion(), d->requestHeaders.minorVersion());
    d->responseHeaders.addValue("CSeq", d->requestHeaders.value("CSeq"));
    QString transport = d->requestHeaders.value("Transport");
    if (!transport.isEmpty())
        d->responseHeaders.addValue("Transport", transport);

    if (!d->sessionId.isEmpty())
        d->responseHeaders.addValue("Session", d->sessionId);
}

void QnRtspConnectionProcessor::generateSessionId()
{
    Q_D(QnRtspConnectionProcessor);
    d->sessionId = QString::number((long) d->socket);
    d->sessionId += QString::number(rand());
}

void QnRtspConnectionProcessor::sendResponse()
{
    Q_D(QnRtspConnectionProcessor);
    if (!d->responseBody.isEmpty())
    {
        d->responseHeaders.setContentLength(d->responseBody.length());
        d->responseHeaders.setContentType("application/sdp");
    }

    QByteArray response = d->responseHeaders.toString().toUtf8();
    response.replace(0,4,"RTSP");
    if (!d->responseBody.isEmpty())
    {
        response += d->responseBody;
        response += "\r\n\r\n";
    }
    d->socket->write(response);
}

int QnRtspConnectionProcessor::numOfVideoChannels()
{
    Q_D(QnRtspConnectionProcessor);
    if (!d->mediaRes)
        return -1;
    QnMediaResourceLayout* layout = d->mediaRes->getMediaLayout();
    return layout ? layout->numberOfVideoChannels() : -1;
}

int QnRtspConnectionProcessor::composeDescribe()
{
    Q_D(QnRtspConnectionProcessor);
    if (!d->mediaRes)
        return CODE_NOT_FOUND;

    QString acceptMethods = d->requestHeaders.value("Accept");
    if (acceptMethods.indexOf("sdp") == -1)
        return CODE_NOT_IMPLEMETED;

    QTextStream sdp(&d->responseBody);
    
    QnMediaResourceLayout* layout = d->mediaRes->getMediaLayout();
    int numVideo = layout->numberOfVideoChannels();
    int numAudio = layout->numberOfAudioChannels();
    for (int i = 0; i < numVideo + numAudio; ++i)
    {
        sdp << "m=" << (i < numVideo ? "video " : "audio ") << i << " RTP/AVP " << RTP_FFMPEG_GENERIC_CODE << ENDL;
        sdp << "a=control:trackID=" << i << ENDL;
        sdp << "a=rtpmap:" << RTP_FFMPEG_GENERIC_CODE << ' ' << RTP_FFMPEG_GENERIC_STR << "/" << CLOCK_FREQUENCY << ENDL;
    }
    return CODE_OK;
}

int QnRtspConnectionProcessor::extractTrackId(const QString& path)
{
    int pos = path.lastIndexOf("/");
    QString trackStr = path.mid(pos+1);
    if (trackStr.toLower().startsWith("trackid"))
    {
        QStringList data = trackStr.split("=");
        if (data.size() > 1)
            return data[1].toInt();
    }
    return -1;
}

int QnRtspConnectionProcessor::composeSetup()
{
    Q_D(QnRtspConnectionProcessor);
    if (!d->mediaRes)
        return CODE_NOT_FOUND;

    QString transport = d->requestHeaders.value("Transport");
    if (transport.indexOf("TCP") == -1)
        return CODE_NOT_IMPLEMETED;
    int trackId = extractTrackId(d->requestHeaders.path());
    if (trackId >= 0)
    {
        QStringList transportInfo = transport.split(';');
        foreach(const QString& data, transportInfo)
        {
            if (data.startsWith("interleaved="))
            {
                QStringList ports = data.mid(QString("interleaved=").length()).split("-");
                d->trackPorts.insert(trackId, QPair<int,int>(ports[0].toInt(), ports.size() > 1 ? ports[1].toInt() : 0));
            }
        }
    }
    return CODE_OK;
}

int QnRtspConnectionProcessor::composePlay()
{
    Q_D(QnRtspConnectionProcessor);
    d->dataProvider = d->mediaRes->addMediaProvider();
    if (!d->dataProvider)
        return CODE_INTERNAL_ERROR;
    if (!d->dataProcessor)
    {
        d->dataProcessor = new QnRtspDataProcessor(this);
        d->dataProvider->addDataProcessor(d->dataProcessor);
        d->dataProvider->start();
        d->dataProcessor->start();
    }
    return CODE_OK;
}

int QnRtspConnectionProcessor::composeTeardown()
{
    Q_D(QnRtspConnectionProcessor);
    d->mediaRes = QnMediaResourcePtr(0);
    return CODE_OK;
}

void QnRtspConnectionProcessor::processRequest()
{
    Q_D(QnRtspConnectionProcessor);
    initResponse();
    QString method = d->requestHeaders.method();
    if (method != "OPTIONS" && d->sessionId.isEmpty())
        generateSessionId();

    if (method == "OPTIONS")
    {
        d->responseHeaders.addValue("Public", "DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, GET_PARAMETER, SET_PARAMETER");
    }
    else if (method == "DESCRIBE")
    {
        composeDescribe();
    }
    else if (method == "SETUP")
    {
        composeSetup();
    }
    else if (method == "PLAY")
    {
        composePlay();
    }
    else if (method == "PAUSE")
    {
    }
    else if (method == "TEARDOWN")
    {
        composeTeardown();
    }
    else if (method == "GET_PARAMETER")
    {
    }
    else if (method == "SET_PARAMETER")
    {
    }
    sendResponse();
}

void QnRtspConnectionProcessor::onClientReadyRead()
{
    Q_D(QnRtspConnectionProcessor);
    d->clientRequest += d->socket->readAll();
    if (isFullMessage())
    {
        parseRequest();
        processRequest();
    }
}
