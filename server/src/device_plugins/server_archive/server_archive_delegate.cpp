#include "server_archive_delegate.h"
#include "core/resourcemanagment/resource_pool.h"
#include "utils/common/util.h"
#include "motion/motion_archive.h"
#include "motion/motion_helper.h"
#include "utils/common/sleep.h"

static const qint64 MOTION_LOAD_STEP = 1000ll * 3600;

QnServerArchiveDelegate::QnServerArchiveDelegate(): 
    QnAbstractArchiveDelegate(),
    m_chunkSequence(0),
    m_aviDelegate(0),
    m_reverseMode(false),
    m_selectedAudioChannel(0),
    m_opened(false),
    m_playbackMaskStart(AV_NOPTS_VALUE),
    m_playbackMaskEnd(AV_NOPTS_VALUE),
    m_lastSeekTime(AV_NOPTS_VALUE),
    m_afterSeek(false),
    m_sendMotion(false),
    m_eof(false)
{
    m_aviDelegate = new QnAviArchiveDelegate();
}

QnServerArchiveDelegate::~QnServerArchiveDelegate()
{
    delete m_chunkSequence;
    delete m_aviDelegate;
}

qint64 QnServerArchiveDelegate::startTime()
{
    return m_catalog ? m_catalog->minTime()*1000 : 0;
}

qint64 QnServerArchiveDelegate::endTime()
{
    return m_catalog  ? m_catalog->maxTime()*1000 : 0;
}

bool QnServerArchiveDelegate::open(QnResourcePtr resource)
{
    if (m_opened)
        return true;
    m_resource = resource;
    QnNetworkResourcePtr netResource = qSharedPointerDynamicCast<QnNetworkResource>(resource);
    Q_ASSERT(netResource != 0);
    m_catalog = qnStorageMan->getFileCatalog(netResource->getMAC().toString());
    m_chunkSequence = new QnChunkSequence(netResource, 0);

    return true;
}

void QnServerArchiveDelegate::close()
{
    m_aviDelegate->close();
    m_catalog = DeviceFileCatalogPtr();
    delete m_chunkSequence;
    m_chunkSequence = 0;
    m_reverseMode = false;
    m_opened = false;
    m_lastSeekTime = AV_NOPTS_VALUE;
    m_afterSeek = false;
}

void QnServerArchiveDelegate::loadPlaybackMask(qint64 msTime)
{
    if (msTime >= m_playbackMaskStart && msTime < m_playbackMaskEnd)
        return;

    qint64 loadMin = msTime - MOTION_LOAD_STEP;
    qint64 loadMax = msTime + MOTION_LOAD_STEP;

    if (!m_playbackMask.isEmpty()) {
        if (m_playbackMaskStart <= loadMin)
            loadMin = m_playbackMask.last().startTimeMs;
        if (m_playbackMaskEnd >= loadMax)
            loadMax = m_playbackMask.first().startTimeMs;
        if (loadMax - loadMin <= 0)
            return;
    }

    QnTimePeriodList newMask = QnMotionHelper::instance()->mathImage(m_motionRegion, m_resource, loadMin, loadMax, 1);
    QVector<QnTimePeriodList> periods;
    periods << m_playbackMask << newMask;
    m_playbackMask = QnTimePeriod::mergeTimePeriods(periods);

    if (!m_playbackMask.isEmpty()) {
        m_playbackMaskStart = qMin(loadMin, m_playbackMask.first().startTimeMs);
        m_playbackMaskEnd = qMin(QDateTime::currentDateTime().toMSecsSinceEpoch(), qMax(loadMax, m_playbackMask.last().startTimeMs + m_playbackMask.last().durationMs));
    }
}

qint64 QnServerArchiveDelegate::correctTimeByMask(qint64 time)
{
    qint64 timeMs = time/1000;

    loadPlaybackMask(timeMs);



    if (timeMs >= m_lastTimePeriod.startTimeMs && timeMs < m_lastTimePeriod.startTimeMs + m_lastTimePeriod.durationMs)
        return time;

    //qDebug() << "inputTime=" << QDateTime::fromMSecsSinceEpoch(time/1000).toString("hh:mm:ss.zzz");

    QnTimePeriodList::iterator itr = qUpperBound(m_playbackMask.begin(), m_playbackMask.end(), timeMs);
    if (itr != m_playbackMask.begin())
    {
        --itr;
        if (itr->startTimeMs + itr->durationMs > timeMs) 
        {
            m_lastTimePeriod = *itr;
            //qDebug() << "found period:" << QDateTime::fromMSecsSinceEpoch(itr->startTimeMs).toString("hh:mm:ss.zzz") << "duration=" << itr->durationMs/1000.0;
        }
        else {
            if (!m_reverseMode)
            {
                ++itr;
                if (itr != m_playbackMask.end())
                {
                    m_lastTimePeriod = *itr;
                    timeMs = itr->startTimeMs;
                    //qDebug() << "correct time to" << QDateTime::fromMSecsSinceEpoch(itr->startTimeMs).toString("hh:mm:ss.zzz");
                }
                else {
                    //qDebug() << "End of motion filter reached" << QDateTime::fromMSecsSinceEpoch(itr->startTimeMs).toString("hh:mm:ss.zzz");
                    return AV_NOPTS_VALUE;
                }
            }
            else {
                m_lastTimePeriod = *itr;
                timeMs = itr->startTimeMs + itr->durationMs;
            }
        }
    }
    else {
        m_lastTimePeriod = *itr;
        //qDebug() << "correct time to" << QDateTime::fromMSecsSinceEpoch(itr->startTimeMs).toString("hh:mm:ss.zzz");
        timeMs = itr->startTimeMs;
    }
    return timeMs*1000;
}

qint64 QnServerArchiveDelegate::seekInternal(qint64 time)
{
    QTime t;
    t.start();

    DeviceFileCatalog::Chunk newChunk = m_chunkSequence->findChunk(m_resource, time, 
        m_reverseMode ? DeviceFileCatalog::OnRecordHole_PrevChunk : DeviceFileCatalog::OnRecordHole_NextChunk);
	/*
    QString s;
    QTextStream str(&s);
    str << "chunk startTime= " << QDateTime::fromMSecsSinceEpoch(newChunk.startTime/1000).toString("hh.mm.ss.zzz") << " duration=" << newChunk.duration/1000000.0;
    str.flush();
    cl_log.log(s, cl_logALWAYS);
	*/

    if (newChunk.startTimeMs != m_currentChunk.startTimeMs)
    {
        bool isStreamsFound = m_aviDelegate->isStreamsFound();
        if (!switchToChunk(newChunk))
            return -1;
        if (isStreamsFound)
            m_aviDelegate->doNotFindStreamInfo(); // optimization
    }

    //int duration = m_currentChunk.duration;
    qint64 chunkOffset = 0;
    if (m_currentChunk.durationMs == -1) // last live chunk
    {
        // do not seek over live chunk because it's very slow (seek always going to begin of file because mkv seek catalog is't ready)
        chunkOffset = qBound(0ll, time - m_currentChunk.startTimeMs*1000, BACKWARD_SEEK_STEP);
    }
    else {
        chunkOffset = qBound(0ll, time - m_currentChunk.startTimeMs*1000, m_currentChunk.durationMs*1000 - BACKWARD_SEEK_STEP);
    }
    qint64 seekRez = m_aviDelegate->seek(chunkOffset);
    if (seekRez == -1)
        return seekRez;
    qint64 rez = m_currentChunk.startTimeMs*1000 + seekRez;
    //cl_log.log("jump time ", t.elapsed(), cl_logALWAYS);
    /*
    QString s;
    QTextStream str(&s);
    str << "server seek:" << QDateTime::fromMSecsSinceEpoch(time/1000).toString("hh:mm:ss.zzz") << " time=" << t.elapsed();
    str.flush();
    cl_log.log(s, cl_logALWAYS);
    */
    m_lastSeekTime = rez;
    m_afterSeek = true;
    return rez;
}

qint64 QnServerArchiveDelegate::seek(qint64 time)
{
    m_tmpData.clear();
    // change time by playback mask
    m_eof = false;
    if (!m_motionRegion.isEmpty()) 
    {
        time = correctTimeByMask(time);
        m_eof = time == AV_NOPTS_VALUE;
        if (m_eof)
            return -1; 
    }
    return seekInternal(time);
}

QnAbstractMediaDataPtr QnServerArchiveDelegate::getNextData()
{
    if (m_eof)
        return QnAbstractMediaDataPtr();

    if (m_tmpData)
    {
        QnAbstractMediaDataPtr rez = m_tmpData;
        m_tmpData.clear();
        return rez;
    }

    int waitMotionCnt = 0;
begin_label:
    QnAbstractMediaDataPtr data = m_aviDelegate->getNextData();
    if (!data)
    {
        DeviceFileCatalog::Chunk chunk;
        do {
            chunk = m_chunkSequence->getNextChunk(m_resource);
            if (chunk.startTimeMs == -1) 
            {
                if (m_reverseMode) {
                    data = QnAbstractMediaDataPtr(new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, 0));
                    data->timestamp = INT64_MAX; // EOF reached
                }
                return data;
            }
        } while (!switchToChunk(chunk));
        data = m_aviDelegate->getNextData();
        if (data)
            data->flags &= ~QnAbstractMediaData::MediaFlags_BOF;
    }
    if (data && !(data->flags & QnAbstractMediaData::MediaFlags_LIVE)) 
    {
        data->timestamp +=m_currentChunk.startTimeMs*1000;

        if (!m_playbackMask.isEmpty()) 
        {
            bool afterSeekIgnore = m_lastSeekTime != AV_NOPTS_VALUE && data->timestamp < m_lastSeekTime;
            if (!afterSeekIgnore)
            {
                qint64 newTime = correctTimeByMask(data->timestamp);
                if (newTime == AV_NOPTS_VALUE)
                {
                    // eof reached
                    if (m_reverseMode) {
                        data = QnAbstractMediaDataPtr(new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, 0));
                        data->timestamp = INT64_MAX; // EOF reached
                    }
                    else {
                        data.clear();
                    }
                    return data;
                }
                if (newTime != data->timestamp) 
                {
                    if (waitMotionCnt > 1)
                        QnSleep::msleep(10);

                    seekInternal(newTime);
                    waitMotionCnt++;
                    goto begin_label;
                }
            }
        }

    }
    if (m_afterSeek)
    {
        data->flags |= QnAbstractMediaData::MediaFlags_BOF;
        m_afterSeek = false;
    }
    
    if (data && m_sendMotion) 
    {
        if (!m_motionConnection)
            m_motionConnection = QnMotionHelper::instance()->createConnection(m_resource);
        QnMetaDataV1Ptr motion = m_motionConnection->getMotionData(data->timestamp);
        if (motion) 
        {
            motion->flags = data->flags;
            motion->opaque = data->opaque;
            m_tmpData = data;
            return motion;
        }
    }
    return data;
}

QnVideoResourceLayout* QnServerArchiveDelegate::getVideoLayout()
{
    return m_aviDelegate->getVideoLayout();
}

QnResourceAudioLayout* QnServerArchiveDelegate::getAudioLayout()
{
    return m_aviDelegate->getAudioLayout();
}

void QnServerArchiveDelegate::onReverseMode(qint64 displayTime, bool value)
{
    m_reverseMode = value;
    m_aviDelegate->onReverseMode(displayTime, value);
}

AVCodecContext* QnServerArchiveDelegate::setAudioChannel(int num)
{
    m_selectedAudioChannel = num;
    return m_aviDelegate->setAudioChannel(num);
}

bool QnServerArchiveDelegate::switchToChunk(const DeviceFileCatalog::Chunk newChunk)
{
    if (newChunk.startTimeMs == -1)
        return false;
    m_currentChunk = newChunk;
    QString url = m_catalog->fullFileName(m_currentChunk);
    m_fileRes = QnAviResourcePtr(new QnAviResource(url));
    m_aviDelegate->close();
    bool rez = m_aviDelegate->open(m_fileRes);
    //if (rez)
    //    m_aviDelegate->setAudioChannel(m_selectedAudioChannel);
    return rez;
}

void QnServerArchiveDelegate::setMotionRegion(const QRegion& region)
{
    if (region != m_motionRegion) {
        m_motionRegion = region;
        m_playbackMaskStart = m_playbackMaskEnd = AV_NOPTS_VALUE;
        m_playbackMask.clear();
    }
}

void QnServerArchiveDelegate::setSendMotion(bool value)
{
    m_sendMotion = value;
}
