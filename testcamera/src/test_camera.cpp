#include "test_camera.h"
#include "utils/media/nalUnits.h"
#include "plugins/resources/test_camera/testcamera_const.h"
#include "utils/common/sleep.h"

class QnFileCache
{
public:
    static QnFileCache* instance();

    QByteArray getMediaData(const QString& fileName)
    {
        QMutexLocker lock(&m_mutex);

        MediaCache::iterator itr = m_cache.find(fileName);
        if (itr != m_cache.end())
            return *itr;

        QFile mediaFile(fileName);
        if (!mediaFile.open(QFile::ReadOnly))
            return QByteArray();
        if (mediaFile.size() > 1024*1024*200)
        {
            qWarning() << "Too large test file. Maximum allowed size is 200Mb";
            return QByteArray();
        }
        QByteArray data = mediaFile.readAll();
        m_cache.insert(fileName, data);
        return data;
    }
    
private:
    typedef QMap<QString, QByteArray> MediaCache;
    MediaCache m_cache;
    QMutex m_mutex;
};

Q_GLOBAL_STATIC(QnFileCache, inst);
QnFileCache* QnFileCache::instance()
{
    return inst();
}


// -------------- QnTestCamera ------------

QnTestCamera::QnTestCamera(quint32 num): m_num(num)
{
    bool ok;
    m_mac = "92-61";
    m_num = htonl(m_num);
    QByteArray last = QByteArray((const char*) &m_num, 4).toHex();
    while (!last.isEmpty()) 
    {
        m_mac += '-';
        m_mac += last.left(2);
        last = last.mid(2);
    }
    m_fps = 30.0;
    m_offlineFreq = 0;
}

QByteArray QnTestCamera::getMac() const
{
    return m_mac;
}

void QnTestCamera::setFileList(const QStringList& files)
{
    m_files = files;
}

void QnTestCamera::setFps(double fps)
{
    m_fps = fps;
}

void QnTestCamera::setOfflineFreq(double offlineFreq)
{
    m_offlineFreq = offlineFreq;
}


const quint8* QnTestCamera::findH264FrameEnd(const quint8* curNal, const quint8* end, bool* isKeyData)
{
    *isKeyData = false;

    const quint8* next = curNal;
    while (next < end)
    {
        if (next < end-m_prefixLen)
        {
            quint8 nalType = next[m_prefixLen] & 0x1f;
            if (nalType == nuSliceIDR)
                *isKeyData = true;
            if (nalType <= nuSliceIDR && next > curNal) 
                return NALUnit::findNALWithStartCode(next+4, end, true);
        }
        next = NALUnit::findNALWithStartCode(next+4, end, true);
    }
    return next;
}

bool QnTestCamera::streamingH264(const QByteArray data, TCPSocket* socket)
{
    const quint8* curNal = (const quint8*) data.data();
    const quint8* end = curNal + data.size();

    m_prefixLen = 3;
    if (curNal < end-4 && curNal[2] == 0)
        m_prefixLen = 4;

    bool isKeyData = false;
    const quint8* nextNal = findH264FrameEnd(curNal, end, &isKeyData);
    double streamingTime = 0;
    QTime timer;
    timer.restart();

    while (curNal < end)
    {
        int sendLen = nextNal-curNal;
        quint32 tmp = htonl(sendLen);
        quint8 codec = TestCamConst::MediaType_H264;

        if (isKeyData)
            codec |= 0x80;

        socket->send(&codec, 1);
        socket->send(&tmp, 4);

        int sended = socket->send(curNal, sendLen);
        if (sended != sendLen)
        {
            qWarning() << "TCP socket write error for camera " << m_mac << "send" << sended << "of" << sendLen;
            return false;
        }
        curNal = nextNal;
        nextNal = findH264FrameEnd(curNal, end, &isKeyData);

        streamingTime += 1000.0 / m_fps;
        int waitingTime = streamingTime - timer.elapsed();
        if (waitingTime > 0)
            QnSleep::msleep(waitingTime);

    }
    return true;
}

bool QnTestCamera::streamingMJPEG(const QByteArray data, TCPSocket* socket)
{
    // todo: implement me
    return false;
}

bool QnTestCamera::doStreamingFile(const QByteArray data, TCPSocket* socket)
{
    if (data.size() < 4)
        return false;
    TestCamConst::MediaType mediaType = TestCamConst::MediaType_MJPEG;
    if (data[0] == 0 && data[1] == 0 && (data[2] == 1 || data[2] == 0 && data[3] == 1))
        mediaType = TestCamConst::MediaType_H264;

    if (mediaType == TestCamConst::MediaType_H264)
        return streamingH264(data, socket);
    else if (mediaType == TestCamConst::MediaType_MJPEG)
        return streamingMJPEG(data, socket);
    else {
        qWarning() << "Unknown media type. Streaming aborted";
        return false;
    }
}

void QnTestCamera::startStreaming(TCPSocket* socket)
{
    int fileIndex = 0;
    while (1)
    {
        QString fileName = m_files[fileIndex];
        QByteArray data = QnFileCache::instance()->getMediaData(fileName);
        if (data.isEmpty())
        {
            qWarning() << "File" << fileName << "not found. Stop streaming for camera" << getMac();
            break;
        }

        if (!doStreamingFile(data, socket))
            break;
        fileIndex = (fileIndex+1) % m_files.size();
    }
}
