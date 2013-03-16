#include "vmax480_archive_delegate.h"
#include "core/resource/resource_media_layout.h"
#include "vmax480_tcp_server.h"
#include "utils/common/sleep.h"

static const int EMPTY_PACKET_REPEAT_INTERVAL = 100;

QnVMax480ArchiveDelegate::QnVMax480ArchiveDelegate(QnResourcePtr res):
    QnAbstractArchiveDelegate(),
    m_needStop(false),
    m_reverseMode(false),
    m_thumbnailsMode(false),
    m_lastSeekPos(AV_NOPTS_VALUE),
    m_isOpened(false),
    m_maxStream(0),
    m_ignoreNextSeek(false),
    m_lastMediaTime(0),
    m_noDataCounter(0)
{
    m_res = res.dynamicCast<QnPlVmax480Resource>();
    m_flags |= Flag_CanOfflineRange;
    m_flags |= Flag_CanProcessNegativeSpeed;
    m_flags |= Flag_CanProcessMediaStep;
    m_flags |= Flag_CanOfflineLayout;
    //m_flags |= Flag_CanSeekImmediatly;
}

QnVMax480ArchiveDelegate::~QnVMax480ArchiveDelegate()
{
    close();
}

bool QnVMax480ArchiveDelegate::open(QnResourcePtr resource)
{
    Q_UNUSED(resource)

    if (m_isOpened)
        return true;

    m_isOpened = true;
    m_needStop = false;

    qDebug() << "before vmaxConnect";

    if (m_maxStream == 0)
        m_maxStream = VMaxStreamFetcher::getInstance(m_groupId, m_res, false);
    int consumerCount = 0;
    m_isOpened = m_maxStream->registerConsumer(this, &consumerCount, !m_thumbnailsMode);
    m_ignoreNextSeek = consumerCount > 1;
    m_noDataCounter = 0;
    return m_isOpened;
}

void QnVMax480ArchiveDelegate::beforeClose()
{
    m_needStop = true;
}

qint64 QnVMax480ArchiveDelegate::seek(qint64 time, bool findIFrame)
{
    m_beforeSeek = false;
    m_lastMediaTime = time;
    if (!m_isOpened) {
        open(m_res);
    }

    if (m_ignoreNextSeek) {
        m_ignoreNextSeek = false;
        return time;
    }

    m_thumbnailsMode = false;
    //QnTimePeriodList chunks = m_res->getChunks();
    //if (!chunks.isEmpty())
    //    time = chunks.roundTimeToPeriodUSec(time, true);

    return seekInternal(time, findIFrame);
}

qint64 QnVMax480ArchiveDelegate::seekInternal(qint64 time, bool findIFrame)
{
    Q_UNUSED(findIFrame)
    qDebug() << "QnVMax480ArchiveDelegate::seek";

    m_maxStream->vmaxArchivePlay(this, time, m_reverseMode ? -1 : 1);
    m_lastSeekPos = time;
    return time;
}

void QnVMax480ArchiveDelegate::close()
{
    m_needStop = true;
    if (m_isOpened) {
        m_maxStream->unregisterConsumer(this);
        m_isOpened = false;
    }
    if (m_maxStream)
        VMaxStreamFetcher::freeInstance(m_groupId, m_res, false);
    m_maxStream = 0;
}

qint64 QnVMax480ArchiveDelegate::startTime()
{
    return m_res->startTime();
}

qint64 QnVMax480ArchiveDelegate::endTime()
{
    return m_res->endTime();
}

void QnVMax480ArchiveDelegate::reconnect()
{
    close();
    open(m_res);
    if (!m_maxStream->isPlaying())
        m_maxStream->vmaxArchivePlay(this, m_lastMediaTime, m_reverseMode ? -1 : 1);
}

QnAbstractMediaDataPtr QnVMax480ArchiveDelegate::getNextData()
{
    QnAbstractMediaDataPtr result;

    
    if (!m_isOpened) {
        open(m_res);
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
    while (m_isOpened) {
        QnAbstractDataPacketPtr tmp = m_maxStream->getNextData(this);
        result = tmp.dynamicCast<QnAbstractMediaData>();

        if (result)
            break;
        if (!m_thumbnailsMode && m_beforeSeek) {
            result = m_maxStream->createEmptyPacket(0); // cancel waiting
            result->flags |= QnAbstractMediaData::MediaFlags_Skip;
            return result;
        }

        if (m_thumbnailsMode) {
            if (getTimer.elapsed() > MAX_FRAME_DURATION*2)
                return result; // tell error
        }
        else {
            if (getTimer.elapsed() > EMPTY_PACKET_REPEAT_INTERVAL) {
                if (++m_noDataCounter == MAX_FRAME_DURATION*2/EMPTY_PACKET_REPEAT_INTERVAL)
                    reconnect();
                return m_maxStream->createEmptyPacket(m_lastMediaTime);
            }
        }

        if (m_needStop)
            break;
    }
    if (result) {
        m_noDataCounter = 0;
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
        m_lastMediaTime = result->timestamp;
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

void QnVMax480ArchiveDelegate::onReverseMode(qint64 displayTime, bool value)
{
    if (m_reverseMode != value) {
        m_reverseMode = value;
        m_ignoreNextSeek = false;
        seek(displayTime, true);
    }
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
    if ((startTime-endTime)/frameStep > 60) {
        qWarning() << "Too large thumbnails range. requested" << (startTime-endTime)/frameStep << "thumbnails. Ignoring";
    }

    qDebug() << "getThumbnails range" << startTime << endTime << frameStep;

    m_thumbnailsMode = true;

    calcSeekPoints(startTime, endTime, frameStep);

    if (m_ThumbnailsSeekPoints.isEmpty())
        return;

    //m_sequence++;
    //vmaxPlayRange(m_ThumbnailsSeekPoints.keys(), m_sequence);
}

int QnVMax480ArchiveDelegate::getChannel() const
{
    return m_res.dynamicCast<QnPhysicalCameraResource>()->getChannel();
}

void QnVMax480ArchiveDelegate::setGroupId(const QByteArray& data)
{
    m_groupId = data;
}

QnTimePeriodList QnVMax480ArchiveDelegate::chunks() 
{ 
    return m_res->getChunks();
}

void QnVMax480ArchiveDelegate::beforeSeek(qint64 time) 
{ 
    m_beforeSeek = true;
}
