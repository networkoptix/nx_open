#include "vmax480_archive_delegate.h"
#include "core/resource/resource_media_layout.h"
#include "vmax480_tcp_server.h"
#include "utils/common/sleep.h"

QnVMax480ArchiveDelegate::QnVMax480ArchiveDelegate(QnResourcePtr res):
    QnAbstractArchiveDelegate(),
    VMaxStreamFetcher(res),
    m_connected(false),
    m_internalQueue(50),
    m_needStop(false),
    m_sequence(0)
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

    m_connected = vmaxConnect(false);
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

    m_internalQueue.clear();
    m_sequence++;
    vmaxArchivePlay(time, m_sequence);
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
    QTime getTimer;
    getTimer.restart();
    while (1) {
        QnAbstractDataPacketPtr tmp;
        m_internalQueue.pop(tmp, 100);
        result = tmp.dynamicCast<QnAbstractMediaData>();
        if (result && result->opaque == m_sequence)
            break;
        if (m_needStop || getTimer.elapsed() > MAX_FRAME_DURATION*1000)
            break;
    }
    if (result)
        result->opaque = 0;
    return result;
}

static QnDefaultResourceVideoLayout m_defaultVideoLayout;

QnResourceVideoLayout* QnVMax480ArchiveDelegate::getVideoLayout()
{
    return &m_defaultVideoLayout;
}

QnResourceAudioLayout* QnVMax480ArchiveDelegate::getAudioLayout()
{
    return 0;
}

void QnVMax480ArchiveDelegate::onGotData(QnAbstractMediaDataPtr mediaData)
{
    m_internalQueue.push(mediaData);
}
