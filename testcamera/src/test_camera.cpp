
#include "test_camera.h"
#include "nx/network/socket.h"

#include <QDebug>

#include <nx/utils/random.h>
#include "nx/streaming/media_data_packet.h"
#include <core/resource/avi/avi_resource.h>
#include <core/resource/avi/avi_archive_delegate.h>
#include "core/resource/test_camera/testcamera_const.h"

#include "utils/common/sleep.h"
#include "utils/media/ffmpeg_helper.h"
#include "utils/media/nalUnits.h"

namespace {
    static const unsigned int kSendTimeoutMs = 1000;
}

QList<QnCompressedVideoDataPtr> QnFileCache::getMediaData(const QString& fileName)
{
    QMutexLocker lock(&m_mutex);
    QList<QnCompressedVideoDataPtr> rez;

    MediaCache::iterator itr = m_cache.find(fileName);
    if (itr != m_cache.end())
        return *itr;

    QnAviResourcePtr file(new QnAviResource(fileName));
    QnAviArchiveDelegate aviDelegate;
    if (!aviDelegate.open(file, nullptr))
    {
        qDebug() << "Can't open file" << fileName;
        return rez;
    }

    QnAbstractMediaDataPtr media;
    qint64 totalSize = 0;

    qDebug() << "Start buffering file" << fileName << "...";

    while (media = aviDelegate.getNextData())
    {
        QnCompressedVideoDataPtr video = std::dynamic_pointer_cast<QnCompressedVideoData>(media);
        if (!video)
            continue;
        rez << video;
        totalSize += video->dataSize();
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
    //bool ok;
    m_mac = "92-61";
    m_num = htonl(m_num);
    QByteArray last = QByteArray((const char*) &m_num, 4).toHex();
    while (!last.isEmpty())
    {
        m_mac += '-';
        m_mac += last.left(2);
        last = last.mid(2);
    }
    m_offlineFreq = 0;
    m_isEnabled = true;
    m_offlineDuration = 0;
    m_checkTimer.restart();
}

QByteArray QnTestCamera::getMac() const
{
    return m_mac;
}

void QnTestCamera::setPrimaryFileList(const QStringList& files)
{
    m_primaryFiles = files;
}

void QnTestCamera::setSecondaryFileList(const QStringList& files)
{
    m_secondaryFiles = files;
}

void QnTestCamera::setOfflineFreq(double offlineFreq)
{
    m_offlineFreq = offlineFreq;
}

int QnTestCamera::sendAll(AbstractStreamSocket* socket, const void* data, int size) {
    int sent = 0, sentTotal = 0;
    while (sentTotal < size) {
        sent = socket->send(static_cast<const quint8*>(data) + sentTotal, size - sentTotal);
        if (sent < 1) {
            qWarning() << "TCP socket '" <<  socket->getForeignAddress().toString() <<
              "' write error for camera" << m_mac << " has sent" << sent << "of" << size;
            SystemError::ErrorCode ercode = 0;
            if (socket->getLastError(&ercode)) {
              qWarning() << "TCP socket '" << socket->getForeignAddress().toString() <<
                "'error code " << ercode;
            }
            break;
        }
        sentTotal += sent;
    }

    return sentTotal == size;
}

bool QnTestCamera::doStreamingFile(QList<QnCompressedVideoDataPtr> data, AbstractStreamSocket* socket, int fps)
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
            QByteArray byteArray = video->context->serialize();

            quint32 packetLen = htonl(byteArray.size());
            quint16 codec = video->compressionType;
            codec |= 0xc000;
            codec = htons(codec);

            if (!sendAll(socket, &codec, 2)) {
                return false;
            }

            if (!sendAll(socket, &packetLen, 4)) {
                return false;
            }

            if (!sendAll(socket, byteArray.data(), byteArray.size())) {
                return false;
            }
        }

        quint32 packetLen = htonl(video->dataSize());
        quint16 codec = video->compressionType;
        if (video->flags & AV_PKT_FLAG_KEY)
            codec |= 0x8000;
        codec = htons(codec);

        if (!sendAll(socket, &codec, 2)) {
            return false;
        }

        if (!sendAll(socket, &packetLen, 4)) {
            return false;
        }

        if (!sendAll(socket, video->data(), video->dataSize())) {
            return false;
        }

        streamingTime += 1000.0 / fps;
        int waitingTime = streamingTime - timer.elapsed();
        if (waitingTime > 0)
            QnSleep::msleep(waitingTime);

    }
    return true;
}

void QnTestCamera::startStreaming(AbstractStreamSocket* socket, bool isSecondary, int fps)
{
    int fileIndex = 0;
    QStringList& fileList = isSecondary ? m_secondaryFiles : m_primaryFiles;
    if (fileList.isEmpty())
        return;

    socket->setSendTimeout(kSendTimeoutMs);

    qDebug() << "Start streaming to " << socket->getForeignAddress().toString();

    while (1)
    {
        QString fileName = fileList[fileIndex];

        QList<QnCompressedVideoDataPtr> data = QnFileCache::instance()->getMediaData(fileName);
        if (data.isEmpty())
        {
            qWarning() << "File" << fileName << "not found. Stop streaming for camera" << getMac();
            break;
        }

        if (!doStreamingFile(data, socket, fps))
            break;
        fileIndex = (fileIndex+1) % fileList.size();
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

    if (m_isEnabled && (nx::utils::random::number(0, 99) < m_offlineFreq))
    {
        m_isEnabled = false;
        m_offlineTimer.restart();
        m_offlineDuration = nx::utils::random::number(2000, 4000);
    }

}

bool QnTestCamera::isEnabled()
{
    makeOfflineFlood();
    return m_isEnabled;
}
