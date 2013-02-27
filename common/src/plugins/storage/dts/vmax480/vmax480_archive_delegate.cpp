#include "vmax480_archive_delegate.h"
#include "core/resource/resource_media_layout.h"
#include "vmax480_tcp_server.h"
#include "utils/common/sleep.h"

QnVMax480ArchiveDelegate::QnVMax480ArchiveDelegate(QnResourcePtr res):
    QnAbstractArchiveDelegate(),
    VMaxStreamFetcher(res),
    m_internalQueue(100),
    m_needStop(false),
    m_sequence(0),
    m_vmaxPaused(false),
    m_lastMediaTime(AV_NOPTS_VALUE),
    m_reverseMode(false),
    m_thumbnailsMode(false)
{
    m_res = res.dynamicCast<QnPlVmax480Resource>();
    m_flags |= Flag_CanOfflineRange;
    m_flags |= Flag_CanProcessNegativeSpeed;
    m_flags |= Flag_CanProcessMediaStep;
}

QnVMax480ArchiveDelegate::~QnVMax480ArchiveDelegate()
{
    vmaxDisconnect();

}

bool QnVMax480ArchiveDelegate::open(QnResourcePtr resource)
{
    if (isOpened())
        return true;

    close();

    m_needStop = false;
    m_sequence = 0;

    int channel = QUrl(m_res->getUrl()).queryItemValue(QLatin1String("channel")).toInt();
    if (channel > 0)
        channel--;

    qDebug() << "before vmaxConnect";

    return vmaxConnect(false, channel);
}

void QnVMax480ArchiveDelegate::beforeClose()
{
    m_needStop = true;
}

qint64 QnVMax480ArchiveDelegate::seek(qint64 time, bool findIFrame)
{
    if (!isOpened())
        return -1;

    m_thumbnailsMode = false;
    QnTimePeriodList chunks = m_res->getChunks();
    if (!chunks.isEmpty())
        time = chunks.roundTimeToPeriodUSec(time, true);

    return seekInternal(time, findIFrame);
}

qint64 QnVMax480ArchiveDelegate::seekInternal(qint64 time, bool findIFrame)
{
    m_sequence++;
    qDebug() << "QnVMax480ArchiveDelegate::seek" << "m_sequence" << m_sequence;

    m_internalQueue.clear();
    vmaxArchivePlay(time, m_sequence, m_reverseMode ? -1 : 1);
    return time;
}

void QnVMax480ArchiveDelegate::close()
{
    m_needStop = true;
    vmaxDisconnect();
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

    if (m_vmaxPaused && m_internalQueue.size() < m_internalQueue.maxSize()/2) {
        vmaxArchivePlay(m_lastMediaTime, m_sequence, 1);
        m_vmaxPaused = false;
    }

    if (m_thumbnailsMode) {
        if (m_ThumbnailsSeekPoints.isEmpty()) {
            close();
            return result;
        }
        seekInternal(m_ThumbnailsSeekPoints.begin().key(), true);
    }

    QTime getTimer;
    getTimer.restart();
    while (isOpened()) {
        QnAbstractDataPacketPtr tmp;
        m_internalQueue.pop(tmp, 100);
        result = tmp.dynamicCast<QnAbstractMediaData>();

        if (result && result->opaque == m_sequence)
            break;
        if (m_needStop || getTimer.elapsed() > MAX_FRAME_DURATION*2)
            return QnAbstractMediaDataPtr();
    }
    if (result) {
        result->opaque = 0;
        if (m_reverseMode) {
            result->flags |= QnAbstractMediaData::MediaFlags_ReverseBlockStart;
            result->flags |= QnAbstractMediaData::MediaFlags_Reverse;
        }

        if (m_thumbnailsMode && !m_ThumbnailsSeekPoints.isEmpty())
        {
            bool isHole = m_ThumbnailsSeekPoints.begin().value();
            if (isHole)
                result->flags |= QnAbstractMediaData::MediaFlags_BOF;

            qDebug() << "got thumbnails.frame=" << QDateTime::fromMSecsSinceEpoch(result->timestamp/1000).toString(QLatin1String("dd hh.mm.ss.zzz"))
                << "point=" << QDateTime::fromMSecsSinceEpoch(m_ThumbnailsSeekPoints.begin().key()/1000).toString(QLatin1String("dd hh.mm.ss.zzz"));

            m_ThumbnailsSeekPoints.erase(m_ThumbnailsSeekPoints.begin());
        }
    }
    else {
        close();
    }

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

    //qDebug() << "timestamp=" << QDateTime::fromMSecsSinceEpoch(mediaData->timestamp/1000).toString(QLatin1String("dd.MM.yyyy hh:mm.ss"));

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

void QnVMax480ArchiveDelegate::onReverseMode(qint64 displayTime, bool value)
{
    m_reverseMode = value;
    seek(displayTime, true);
}

void QnVMax480ArchiveDelegate::calcSeekPoints(qint64 startTime, qint64 endTime, qint64 frameStep)
{
    qint64 curTime = startTime;
    QnTimePeriodList chunks = m_res->getChunks();
    while (1) 
    {
        qint64 seekRez = chunks.roundTimeToPeriodUSec(curTime, true);
        if (seekRez > endTime)
            break;
        bool holeDetected = seekRez > curTime;
        curTime = seekRez - (seekRez-curTime)%frameStep;

        m_ThumbnailsSeekPoints.insert(seekRez, holeDetected);

        curTime += frameStep;
    }
}

void QnVMax480ArchiveDelegate::setRange(qint64 startTime, qint64 endTime, qint64 frameStep)
{
    qDebug() << "getThumbnails range" << startTime << endTime << frameStep;

    m_thumbnailsMode = true;

    calcSeekPoints(startTime, endTime, frameStep);

    if (m_ThumbnailsSeekPoints.isEmpty())
        return;

    open(m_res);
    if (!isOpened())
        return;

    //m_sequence++;
    //vmaxPlayRange(m_ThumbnailsSeekPoints.keys(), m_sequence);
}
