#include <QDebug>

#include "test_camera.h"
#include "utils/media/nalUnits.h"
#include "plugins/resources/test_camera/testcamera_const.h"
#include "utils/common/sleep.h"
#include "core/datapacket/mediadatapacket.h"
#include "plugins/resources/archive/avi_files/avi_resource.h"
#include "plugins/resources/archive/avi_files/avi_archive_delegate.h"
#include "utils/media/ffmpeg_helper.h"

QList<QnCompressedVideoDataPtr> QnFileCache::getMediaData(const QString& fileName)
{
    QMutexLocker lock(&m_mutex);
    QList<QnCompressedVideoDataPtr> rez;

    MediaCache::iterator itr = m_cache.find(fileName);
    if (itr != m_cache.end())
        return *itr;

    QnAviResourcePtr file(new QnAviResource(fileName));
    QnAviArchiveDelegate aviDelegate;
    if (!aviDelegate.open(file))
    {
        qDebug() << "Can't open file" << fileName;
        return rez;
    }

    QnAbstractMediaDataPtr media;
    qint64 totalSize = 0;

    qDebug() << "Start buffering file" << fileName << "...";

    while (media = aviDelegate.getNextData())
    {
        QnCompressedVideoDataPtr video = qSharedPointerDynamicCast<QnCompressedVideoData>(media);
        if (!video)
            continue;
        rez << video;
        totalSize += video->data.size();
        if (totalSize > 1024*1024*100)
        {
            qDebug() << "File" << fileName << "too large. Using first 100M.";
            break;
        }
    }
    qDebug() << "File" << fileName << "ready for streaming";
    m_cache.insert(fileName, rez);
    return rez;
}

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
    m_isEnabled = true;
    m_offlineDuration = 0;
    m_checkTimer.restart();
    srand(QDateTime::currentMSecsSinceEpoch());
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

bool QnTestCamera::doStreamingFile(QList<QnCompressedVideoDataPtr> data, TCPSocket* socket)
{
    double streamingTime = 0;
    QTime timer;
    timer.restart();

    for (int i = 0; i < data.size(); ++i)
    {

        makeOfflineFlood();
        if (!m_isEnabled) {
            QnSleep::msleep(100);
            return false;
        }

        QnCompressedVideoDataPtr video = data[i];
        if (i == 0)
        {
            //m_context = QnMediaContextPtr( new QnMediaContext(video->context));
            QByteArray byteArray;
            QnFfmpegHelper::serializeCodecContext(video->context->ctx(), &byteArray);

            quint32 packetLen = htonl(byteArray.size());
            quint16 codec = video->compressionType;
            codec |= 0xc000;
            codec = htons(codec);

            socket->send(&codec, 2);
            socket->send(&packetLen, 4);

            int sended = socket->send(byteArray.data(), byteArray.size());
        }

        quint32 packetLen = htonl(video->data.size());
        quint16 codec = video->compressionType;
        if (video->flags & AV_PKT_FLAG_KEY)
            codec |= 0x8000;
        codec = htons(codec);

        socket->send(&codec, 2);
        socket->send(&packetLen, 4);

        int sended = socket->send(video->data.data(), video->data.size());
        if (sended != video->data.size())
        {
            qWarning() << "TCP socket write error for camera " << m_mac << "send" << sended << "of" << video->data.size();
            return false;
        }

        streamingTime += 1000.0 / m_fps;
        int waitingTime = streamingTime - timer.elapsed();
        if (waitingTime > 0)
            QnSleep::msleep(waitingTime);

    }
    return true;
}

void QnTestCamera::startStreaming(TCPSocket* socket)
{
    int fileIndex = 0;
    while (1)
    {
        QString fileName = m_files[fileIndex];
        QList<QnCompressedVideoDataPtr> data = QnFileCache::instance()->getMediaData(fileName);
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

void QnTestCamera::makeOfflineFlood()
{
    if (m_checkTimer.elapsed() < 1000)
        return;
    m_checkTimer.restart();

    if (!m_isEnabled)
    {
        if (m_offlineTimer.elapsed() > m_offlineDuration) {
            m_isEnabled = true;
            return;
        }
    }

    if (m_isEnabled && (rand() % 100 < m_offlineFreq))
    {
        m_isEnabled = false;
        m_offlineTimer.restart();
        m_offlineDuration = 2000 + (rand() % 2000);
    }

}

bool QnTestCamera::isEnabled()
{
    makeOfflineFlood();
    return m_isEnabled;
}
