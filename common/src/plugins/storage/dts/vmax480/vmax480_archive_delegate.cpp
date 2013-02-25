#include "vmax480_archive_delegate.h"
#include "core/resource/resource_media_layout.h"
#include "vmax480_tcp_server.h"
#include "utils/common/sleep.h"

QnVMax480ArchiveDelegate::QnVMax480ArchiveDelegate(QnResourcePtr res):
    QnAbstractArchiveDelegate(),
    VMaxStreamFetcher(res),
    m_connected(false),
    m_internalQueue(100),
    m_needStop(false),
    m_sequence(0),
    m_vmaxPaused(false),
    m_lastMediaTime(AV_NOPTS_VALUE),
    m_singleShotMode(false)
{
    m_res = res.dynamicCast<QnPlVmax480Resource>();
    m_flags |= Flag_CanOfflineRange;
}

QnVMax480ArchiveDelegate::~QnVMax480ArchiveDelegate()
{
    vmaxDisconnect();

}

bool QnVMax480ArchiveDelegate::open(QnResourcePtr resource)
{
    m_needStop = false;

    if (m_connected)
        return true;

    m_sequence = 0;

    int channel = QUrl(m_res->getUrl()).queryItemValue(QLatin1String("channel")).toInt();
    if (channel > 0)
        channel--;
    m_connected = vmaxConnect(false, channel);
    return m_connected;
}

void QnVMax480ArchiveDelegate::beforeClose()
{
    m_needStop = true;
}

qint64 QnVMax480ArchiveDelegate::seek(qint64 time, bool findIFrame)
{
    if (!isOpened())
        return -1;

    m_sequence++;
    m_internalQueue.clear();
    vmaxArchivePlay(time, m_sequence, 1);
    return time;
}

void QnVMax480ArchiveDelegate::close()
{
    m_needStop = true;
    vmaxDisconnect();
    m_connected =false;
}

qint64 QnVMax480ArchiveDelegate::startTime()
{
    return m_res->startTime();
}

qint64 QnVMax480ArchiveDelegate::endTime()
{
    return m_res->endTime();
}

QnAbstractMediaDataPtr QnVMax480ArchiveDelegate::getNextData()
{
    QnAbstractMediaDataPtr result;

    if (!m_singleShotMode && m_vmaxPaused && m_internalQueue.size() < m_internalQueue.maxSize()/2) {
        vmaxArchivePlay(m_lastMediaTime, m_sequence, 1);
        m_vmaxPaused = false;
    }

    QTime getTimer;
    getTimer.restart();
    while (1) {
        QnAbstractDataPacketPtr tmp;
        m_internalQueue.pop(tmp, 100);
        result = tmp.dynamicCast<QnAbstractMediaData>();
        if (result && result->opaque == m_sequence)
            break;
        if (m_needStop || getTimer.elapsed() > MAX_FRAME_DURATION*1000)
            return QnAbstractMediaDataPtr();
    }
    if (result)
        result->opaque = 0;
    return result;
}

static QnDefaultResourceVideoLayout videoLayout;
QnResourceVideoLayout* QnVMax480ArchiveDelegate::getVideoLayout()
{
    return &videoLayout;
}

static QnEmptyResourceAudioLayout audioLayout;
QnResourceAudioLayout* QnVMax480ArchiveDelegate::getAudioLayout()
{
    return &audioLayout;
}

void QnVMax480ArchiveDelegate::onGotData(QnAbstractMediaDataPtr mediaData)
{
    QTime waitTimer;
    waitTimer.restart();
    while (!m_needStop && m_internalQueue.size() > m_internalQueue.maxSize() && !m_vmaxPaused)
    {
        if (waitTimer.elapsed() > MAX_FRAME_DURATION) {
            if (!m_vmaxPaused) {
                vmaxArchivePlay(m_lastMediaTime, m_sequence, 0);
                m_vmaxPaused = true;
            }
        }
        else {
            QnSleep::msleep(1);
        }
    }

    m_internalQueue.push(mediaData);
    m_lastMediaTime = mediaData->timestamp;
}
