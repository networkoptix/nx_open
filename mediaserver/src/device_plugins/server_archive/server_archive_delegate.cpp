#include "server_archive_delegate.h"
#include "core/resourcemanagment/resource_pool.h"
#include "utils/common/util.h"
#include "motion/motion_archive.h"
#include "motion/motion_helper.h"
#include "utils/common/sleep.h"

static const qint64 MOTION_LOAD_STEP = 1000ll * 3600;
static const int SECOND_STREAM_FIND_EPS = 1000 * 5;

QnServerArchiveDelegate::QnServerArchiveDelegate(): 
    QnAbstractArchiveDelegate(),
    m_chunkSequenceHi(0),
    m_chunkSequenceLow(0),
    m_reverseMode(false),
    m_selectedAudioChannel(0),
    m_opened(false),
    m_playbackMaskStart(AV_NOPTS_VALUE),
    m_playbackMaskEnd(AV_NOPTS_VALUE),
    m_lastSeekTime(AV_NOPTS_VALUE),
    m_afterSeek(false),
    m_sendMotion(false),
    m_eof(false),
    m_quality(MEDIA_Quality_High),
    m_skipFramesToTime(0),
    m_lastPacketTime(0)
{
    m_aviDelegate = QnAviArchiveDelegatePtr(new QnAviArchiveDelegate());
    m_aviDelegate->setUseAbsolutePos(false);

    m_newQualityAviDelegate = QnAviArchiveDelegatePtr(0);
}

QnServerArchiveDelegate::~QnServerArchiveDelegate()
{
    delete m_chunkSequenceHi;
    delete m_chunkSequenceLow;
}

qint64 QnServerArchiveDelegate::startTime()
{
	if (!m_catalogHi || m_catalogHi->minTime() == AV_NOPTS_VALUE)
		return AV_NOPTS_VALUE;
    return m_catalogHi->minTime()*1000;
}

qint64 QnServerArchiveDelegate::endTime()
{
    return m_catalogHi  ? m_catalogHi->maxTime()*1000 : 0;
}

bool QnServerArchiveDelegate::open(QnResourcePtr resource)
{
    if (m_opened)
        return true;
    m_resource = resource;
    QnNetworkResourcePtr netResource = qSharedPointerDynamicCast<QnNetworkResource>(resource);
    Q_ASSERT(netResource != 0);
    m_catalogHi = qnStorageMan->getFileCatalog(netResource->getMAC().toString(), QnResource::Role_LiveVideo);
    m_catalogLow = qnStorageMan->getFileCatalog(netResource->getMAC().toString(), QnResource::Role_SecondaryLiveVideo);
    m_chunkSequenceHi = new QnChunkSequence(netResource, QnResource::Role_LiveVideo, 0);
    m_chunkSequenceLow = new QnChunkSequence(netResource, QnResource::Role_SecondaryLiveVideo, 0);

    m_currentChunkCatalog = m_quality == MEDIA_Quality_High ? m_catalogHi : m_catalogLow;

    return true;
}

void QnServerArchiveDelegate::close()
{
    m_currentChunkCatalog.clear();
    m_aviDelegate->close();
    m_catalogHi.clear();
    m_catalogLow.clear();
    delete m_chunkSequenceHi;
    delete m_chunkSequenceLow;
    m_chunkSequenceHi = 0;
    m_chunkSequenceLow = 0;
    m_reverseMode = false;
    m_opened = false;
    m_lastSeekTime = AV_NOPTS_VALUE;
    m_afterSeek = false;
    m_skipFramesToTime = 0;
    m_lastPacketTime = 0;
}

void QnServerArchiveDelegate::loadPlaybackMask(qint64 msTime, bool useReverseSearch)
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

    bool dataFound = false;
    QnMotionArchive* archive = QnMotionHelper::instance()->getArchive(m_resource);
    while (!dataFound)
    {
        QnTimePeriodList newMask = archive->mathPeriod(m_motionRegion, loadMin, loadMax, 1);
        if (!newMask.isEmpty())
        {
            QVector<QnTimePeriodList> periods;
            periods << m_playbackMask << newMask;
            m_playbackMask = QnTimePeriod::mergeTimePeriods(periods);
            dataFound = true;
        }
        if (m_playbackMaskStart != AV_NOPTS_VALUE) {
            m_playbackMaskStart = qMin(loadMin, m_playbackMaskStart);
            m_playbackMaskEnd = qMin(archive->maxTime(), qMax(loadMax, m_playbackMaskEnd));
        }
        else {
            m_playbackMaskStart = loadMin;
            m_playbackMaskEnd = qMin(archive->maxTime(), loadMax);
        }

        if (!useReverseSearch) {
            loadMin = loadMax;
            loadMax += MOTION_LOAD_STEP;
            if (loadMin >= archive->maxTime())
                break;
        }
        else {
            loadMax = loadMin;
            loadMin -= MOTION_LOAD_STEP;
            if (loadMax <= archive->minTime())
                break;
        }
    }

    /*
    if (!m_playbackMask.isEmpty()) {
        m_playbackMaskStart = qMin(loadMin, m_playbackMask.first().startTimeMs);
        m_playbackMaskEnd = qMin(QDateTime::currentDateTime().toMSecsSinceEpoch(), qMax(loadMax, m_playbackMask.last().startTimeMs + m_playbackMask.last().durationMs));
    }
    */
}

qint64 QnServerArchiveDelegate::correctTimeByMask(qint64 time, bool useReverseSearch)
{
    qint64 timeMs = time/1000;

    loadPlaybackMask(timeMs, useReverseSearch);



    if (timeMs >= m_lastTimePeriod.startTimeMs && timeMs < m_lastTimePeriod.startTimeMs + m_lastTimePeriod.durationMs)
        return time;

    if (m_playbackMask.isEmpty())
        return AV_NOPTS_VALUE;

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
            if (!useReverseSearch)
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
                timeMs = qMax(itr->startTimeMs, itr->startTimeMs + itr->durationMs - BACKWARD_SEEK_STEP);
                //qDebug() << "correct time to" << QDateTime::fromMSecsSinceEpoch(timeMs).toString("hh:mm:ss.zzz");
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

qint64 QnServerArchiveDelegate::seekInternal(qint64 time, bool findIFrame, bool recursive)
{
    //QTime t;
    //t.start();
    m_skipFramesToTime = 0;
    qint64 timeMs = time/1000;
    m_newQualityTmpData.clear();
    DeviceFileCatalog::FindMethod findMethod = m_reverseMode ? DeviceFileCatalog::OnRecordHole_PrevChunk : DeviceFileCatalog::OnRecordHole_NextChunk;
    DeviceFileCatalog::Chunk newChunk;
    DeviceFileCatalogPtr newChunkCatalog;
    if (m_quality == MEDIA_Quality_Low)
    {
        newChunk = m_chunkSequenceLow->findChunk(m_resource, timeMs, findMethod);
        newChunkCatalog = m_catalogLow;
        int distanceLow = newChunk.distanceToTime(timeMs);
        if (distanceLow > 0) 
        {
            // Low quality chunk not found exactly for requested time. So, find chunk in high quality sequence
            DeviceFileCatalog::Chunk newChunkPrimary = m_chunkSequenceHi->findChunk(m_resource, timeMs, findMethod);
            int distanceHi = newChunkPrimary.distanceToTime(timeMs);
            if (distanceLow - distanceHi > SECOND_STREAM_FIND_EPS)
            {
                newChunk = newChunkPrimary;
                newChunkCatalog = m_catalogHi;
            }
        }
    }
    else {
        newChunk = m_chunkSequenceHi->findChunk(m_resource, timeMs, findMethod);
        newChunkCatalog = m_catalogHi;
    }

	/*
    QString s;
    QTextStream str(&s);
    str << "chunk startTime= " << QDateTime::fromMSecsSinceEpoch(newChunk.startTime/1000).toString("hh.mm.ss.zzz") << " duration=" << newChunk.duration/1000000.0;
    str.flush();
    cl_log.log(s, cl_logALWAYS);
	*/

    qint64 chunkOffset = 0;
    if (newChunk.durationMs == -1) // last live chunk
    {
        // do not seek over live chunk because it's very slow (seek always going to begin of file because mkv seek catalog is't ready)
        chunkOffset = qBound(0ll, time - newChunk.startTimeMs*1000, BACKWARD_SEEK_STEP);

        // New condition: Also, last file may has zerro size (because of file write buffering), and read operation will return fail
        // It is contains some troubles for REF from live. So avoid seek to last file at all.
        if (m_reverseMode && recursive)
            return seekInternal(newChunk.startTimeMs*1000 - BACKWARD_SEEK_STEP, findIFrame, false);

    }
    else {
        chunkOffset = qBound(0ll, time - newChunk.startTimeMs*1000, newChunk.durationMs*1000 - BACKWARD_SEEK_STEP);
    }

    if (newChunk.startTimeMs != m_currentChunk.startTimeMs)
    {
        bool isStreamsFound = m_aviDelegate->isStreamsFound();
        if (!switchToChunk(newChunk, newChunkCatalog))
            return -1;
        if (isStreamsFound)
            m_aviDelegate->doNotFindStreamInfo(); // optimization
    }

    qint64 seekRez = m_aviDelegate->seek(chunkOffset, findIFrame);
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

qint64 QnServerArchiveDelegate::seek(qint64 time, bool findIFrame)
{
    m_tmpData.clear();
    // change time by playback mask
    m_eof = false;
    if (!m_motionRegion.isEmpty()) 
    {
        time = correctTimeByMask(time, m_reverseMode);
        m_eof = time == AV_NOPTS_VALUE;
        if (m_eof)
            return -1; 
    }
    return seekInternal(time, findIFrame, true);
}

void QnServerArchiveDelegate::getNextChunk(DeviceFileCatalog::Chunk& chunk, DeviceFileCatalogPtr& chunkCatalog)
{
    m_newQualityTmpData.clear();

    m_skipFramesToTime = 0;
    bool isCatalogEqualQuality = (m_quality == MEDIA_Quality_High && m_currentChunkCatalog == m_catalogHi) ||
                                 (m_quality == MEDIA_Quality_Low && m_currentChunkCatalog == m_catalogLow);

    qint64 prevEndTimeMs = m_currentChunk.startTimeMs + m_currentChunk.durationMs;
    QnChunkSequence* currentSequence = m_currentChunkCatalog == m_catalogHi ? m_chunkSequenceHi : m_chunkSequenceLow;

    if (m_currentChunk.durationMs == -1)
    {
        // last chunk. Find same chunk again (possible it chunk already finished)
        chunk = currentSequence->findChunk(m_resource, m_currentChunk.startTimeMs, DeviceFileCatalog::OnRecordHole_NextChunk);
        if (chunk.durationMs == -1) {
            chunk.startTimeMs = -1;
            return;
        }
        prevEndTimeMs = chunk.startTimeMs + chunk.durationMs;
    }

    //chunk = currentSequence->getNextChunk(m_resource);
    // todo: getNextChunk is faster, but need more logic because of 2 chunk sequence now. So, speed may be improved
    chunk = currentSequence->findChunk(m_resource, prevEndTimeMs, DeviceFileCatalog::OnRecordHole_NextChunk);
    
    chunkCatalog = m_currentChunkCatalog;

    if (chunk.startTimeMs == -1)
        return; // EOF reached. do not switch quality
    if (isCatalogEqualQuality) 
    {
        if (m_currentChunkCatalog == m_catalogLow)
        {
            qint64 lowDistance = chunk.distanceToTime(prevEndTimeMs);
            if (lowDistance > SECOND_STREAM_FIND_EPS)
            {
                // Hole in a low quality chunks. Try to find chunk in a high quality
                // Find Hi chunk slightly in a future because of Hi/Low chunks endians not full sync 
                // (if chunk start will be found in the past, so at least SECOND_STREAM_FIND_EPS data can be used)
                const DeviceFileCatalog::Chunk& chunkHi =  m_chunkSequenceHi->findChunk(m_resource, prevEndTimeMs+SECOND_STREAM_FIND_EPS, DeviceFileCatalog::OnRecordHole_NextChunk);
                qint64 hiDistance = chunkHi.distanceToTime(prevEndTimeMs);
                if (hiDistance < lowDistance && lowDistance - hiDistance > SECOND_STREAM_FIND_EPS)
                {
                    // data found in high quality chunks. Switching
                    chunk = chunkHi;
                    chunkCatalog = m_catalogHi;
                    if (chunk.startTimeMs < prevEndTimeMs)
                        m_skipFramesToTime = prevEndTimeMs*1000;
                }
            }
        }
    }
    else if (m_currentChunkCatalog == m_catalogHi)
    {
        // Quality is Low, but current catalog is High because not low data for current period.
        // Checing for switching to a low quality
        const DeviceFileCatalog::Chunk& chunkLow =  m_chunkSequenceLow->findChunk(m_resource, chunk.startTimeMs, DeviceFileCatalog::OnRecordHole_NextChunk);
        qint64 lowDistance = chunkLow.distanceToTime(chunk.startTimeMs);
        qint64 hiDistance = chunk.distanceToTime(prevEndTimeMs);
        if (hiDistance < lowDistance && lowDistance - hiDistance > SECOND_STREAM_FIND_EPS)
            return; // no low quality data appear

        // low quality data found. Switching
        if (chunk.startTimeMs > chunkLow.startTimeMs)
            m_skipFramesToTime = chunk.startTimeMs*1000;
        chunk = chunkLow;
        chunkCatalog = m_catalogLow;
    }
    else {
        // Switch to hiQuality
        const DeviceFileCatalog::Chunk& chunkHi =  m_chunkSequenceHi->findChunk(m_resource, prevEndTimeMs, DeviceFileCatalog::OnRecordHole_NextChunk);
        if (chunkHi.startTimeMs < prevEndTimeMs)
            m_skipFramesToTime = prevEndTimeMs*1000;
        chunk = chunkHi;
        chunkCatalog = m_catalogHi;
    }
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
        DeviceFileCatalogPtr chunkCatalog;
        do {
            getNextChunk(chunk, chunkCatalog);
            if (chunk.startTimeMs == -1) 
            {
                if (m_reverseMode) {
                    data = QnAbstractMediaDataPtr(new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, 0));
                    data->timestamp = INT64_MAX; // EOF reached
                }
                return data;
            }
        } while (!switchToChunk(chunk, chunkCatalog));

        if (m_skipFramesToTime != 0)
        {
            m_aviDelegate->seek(m_skipFramesToTime, true);
        }

        data = m_aviDelegate->getNextData();
        if (data) 
            data->flags &= ~QnAbstractMediaData::MediaFlags_BOF;
    }
    if (data && !(data->flags & QnAbstractMediaData::MediaFlags_LIVE)) 
    {
        data->timestamp +=m_currentChunk.startTimeMs*1000;
        m_lastPacketTime = data->timestamp;

        if (m_skipFramesToTime) {
            if (data->timestamp < m_skipFramesToTime)
                data->flags |= QnAbstractMediaData::MediaFlags_Ignore;
            else
                m_skipFramesToTime = 0;
        }

        // Switch quality on I-frame (slow switching without seek) check
        if (m_newQualityTmpData && m_newQualityTmpData->timestamp <= data->timestamp)
        {
            m_currentChunk = m_newQualityChunk;
            m_currentChunkCatalog = m_newQualityCatalog;
            data = m_newQualityTmpData;
            m_newQualityTmpData.clear();
            m_aviDelegate = m_newQualityAviDelegate;
            m_newQualityAviDelegate.clear();
            m_fileRes = m_newQualityFileRes;
            m_newQualityFileRes.clear();
        }


        if (!m_playbackMask.isEmpty()) 
        {
            bool afterSeekIgnore = m_lastSeekTime != AV_NOPTS_VALUE && data->timestamp < m_lastSeekTime;
            if (!afterSeekIgnore)
            {
                qint64 newTime = correctTimeByMask(data->timestamp, false);
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

                    seekInternal(newTime, true, true);
                    waitMotionCnt++;
                    goto begin_label;
                }
            }
        }

    }
    if (data)
    {
        if (m_afterSeek)
            data->flags |= QnAbstractMediaData::MediaFlags_BOF;
        m_afterSeek = false;
        if (m_currentChunkCatalog == m_catalogLow)
            data->flags |= QnAbstractMediaData::MediaFlags_LowQuality;
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

bool QnServerArchiveDelegate::switchToChunk(const DeviceFileCatalog::Chunk newChunk, DeviceFileCatalogPtr newCatalog)
{
    if (newChunk.startTimeMs == -1)
        return false;
    m_currentChunk = newChunk;
    m_currentChunkCatalog = newCatalog;
    QString url = m_currentChunkCatalog->fullFileName(m_currentChunk);
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

bool QnServerArchiveDelegate::setQuality(MediaQuality quality, bool fastSwitch)
{
    m_quality = quality;
    m_newQualityTmpData.clear();

    if (!fastSwitch)
    {
        m_newQualityCatalog = quality == MEDIA_Quality_High ? m_catalogHi : m_catalogLow;
        QnChunkSequence* chunkSequence = quality == MEDIA_Quality_High ? m_chunkSequenceHi : m_chunkSequenceLow;
        qint64 timeMs = m_lastPacketTime/1000 + 1;
        m_newQualityChunk = chunkSequence->findChunk(m_resource, timeMs, DeviceFileCatalog::OnRecordHole_NextChunk);
        if (m_newQualityChunk.distanceToTime(timeMs) > SECOND_STREAM_FIND_EPS) {

            return false;
        }
        QString url = m_newQualityCatalog->fullFileName(m_newQualityChunk);
        m_newQualityFileRes = QnAviResourcePtr(new QnAviResource(url));
        m_newQualityAviDelegate = QnAviArchiveDelegatePtr(new QnAviArchiveDelegate());
        m_newQualityAviDelegate->setUseAbsolutePos(false);

        if (!m_newQualityAviDelegate->open(m_newQualityFileRes))
            return false;
        qint64 chunkOffset = (timeMs - m_newQualityChunk.startTimeMs)*1000;
        m_newQualityAviDelegate->doNotFindStreamInfo();
        if (!m_newQualityAviDelegate->seek(chunkOffset, false))
            return false;

        while (1) {
            m_newQualityTmpData = m_newQualityAviDelegate->getNextData();
            if (m_newQualityTmpData == 0) 
            {
                qDebug() << "switching data not found. Chunk start=" << QDateTime::fromMSecsSinceEpoch(m_newQualityChunk.startTimeMs).toString();
                qDebug() << "requiredTime=" << QDateTime::fromMSecsSinceEpoch(m_lastPacketTime/1000.0).toString();
                break;
            }
            m_newQualityTmpData->timestamp += m_newQualityChunk.startTimeMs*1000;
            qDebug() << "switching data. skip time=" << QDateTime::fromMSecsSinceEpoch(m_newQualityTmpData->timestamp/1000).toString() << "flags=" << (m_newQualityTmpData->flags & AV_PKT_FLAG_KEY);
            if (m_newQualityTmpData->timestamp >= m_lastPacketTime && (m_newQualityTmpData->flags & AV_PKT_FLAG_KEY))
                break;
        }
    }

    return fastSwitch;
    /*
    if (fastSwitch)
    if (m_lastPacketTime != 0) {
        seekInternal(m_lastPacketTime + 1, true, true);
        m_skipFramesToTime = m_lastPacketTime + 1;
    }
    */
}
