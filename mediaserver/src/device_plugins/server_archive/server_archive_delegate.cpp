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
    m_reverseMode(false),
    m_selectedAudioChannel(0),
    m_opened(false),
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

    m_currentChunkCatalog = m_quality == MEDIA_Quality_High ? m_catalogHi : m_catalogLow;

    return true;
}

void QnServerArchiveDelegate::close()
{
    m_currentChunkCatalog.clear();
    m_aviDelegate->close();
    m_catalogHi.clear();
    m_catalogLow.clear();
    //m_reverseMode = false;
    m_opened = false;
    m_lastSeekTime = AV_NOPTS_VALUE;
    m_afterSeek = false;
    m_skipFramesToTime = 0;
    m_lastPacketTime = 0;
}

qint64 QnServerArchiveDelegate::seekInternal(qint64 time, bool findIFrame, bool recursive)
{
    //QTime t;
    //t.start();
    m_skipFramesToTime = 0;
    qint64 timeMs = time/1000;
    m_newQualityTmpData.clear();
    m_newQualityAviDelegate.clear();

    DeviceFileCatalog::FindMethod findMethod = m_reverseMode ? DeviceFileCatalog::OnRecordHole_PrevChunk : DeviceFileCatalog::OnRecordHole_NextChunk;
    DeviceFileCatalog::Chunk newChunk;
    DeviceFileCatalogPtr newChunkCatalog;
    if (m_quality == MEDIA_Quality_Low)
    {
        newChunk = findChunk(m_catalogLow, timeMs, findMethod);
        newChunkCatalog = m_catalogLow;
        int distanceLow = newChunk.distanceToTime(timeMs);
        if (distanceLow > 0) 
        {
            // Low quality chunk not found exactly for requested time. So, find chunk in high quality sequence
            DeviceFileCatalog::Chunk newChunkPrimary = findChunk(m_catalogHi, timeMs, findMethod); 
            int distanceHi = newChunkPrimary.distanceToTime(timeMs);
            if (distanceLow - distanceHi > SECOND_STREAM_FIND_EPS)
            {
                newChunk = newChunkPrimary;
                newChunkCatalog = m_catalogHi;
            }
        }
    }
    else {
        newChunk = findChunk(m_catalogHi, timeMs, findMethod);
        newChunkCatalog = m_catalogHi;
    }

	
    //qDebug() << "seekTo= " << QDateTime::fromMSecsSinceEpoch(timeMs).toString("hh.mm.ss.zzz");
    //qDebug() << "chunk startTime= " << QDateTime::fromMSecsSinceEpoch(newChunk.startTimeMs).toString("hh.mm.ss.zzz") << " duration=" << newChunk.durationMs/1000.0;

    qint64 chunkOffset = 0;
    if (newChunk.durationMs == -1) // last live chunk
    {
        if (!m_reverseMode && time - newChunk.startTimeMs*1000 > 1000 * 1000ll) 
        {
            // seek > 1 seconds offset at last non-closed chunk. 
            // Ignore seek, and go to EOF (actually seek always goes to the begin of the file if file is not closed, so it is slow)
            m_eof = true;
            return newChunk.startTimeMs*1000;
        }

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
        //time = correctTimeByMask(time, m_reverseMode);
        m_eof = time == AV_NOPTS_VALUE;
        if (m_eof)
            return -1; 
    }
    return seekInternal(time, findIFrame, true);
}

DeviceFileCatalog::Chunk QnServerArchiveDelegate::findChunk(DeviceFileCatalogPtr catalog, qint64 time, DeviceFileCatalog::FindMethod findMethod)
{
    int index = catalog->findFileIndex(time, findMethod);
    return catalog->chunkAt(index);
}

void QnServerArchiveDelegate::getNextChunk(DeviceFileCatalog::Chunk& chunk, DeviceFileCatalogPtr& chunkCatalog)
{
    //m_newQualityTmpData.clear();
    //m_newQualityAviDelegate.clear();

    m_skipFramesToTime = 0;

    qint64 prevEndTimeMs = m_currentChunk.startTimeMs + m_currentChunk.durationMs;

    // check for last chunk
    if (m_currentChunkCatalog->isLastChunk(m_currentChunk.startTimeMs))
    {
        if (m_currentChunkCatalog == m_catalogHi || m_currentChunk.durationMs == -1)
        {
            // EOF reached
            chunk.startTimeMs = -1;
            return;
        }
        else 
        {
            // It is last and closed chunk for low quality. Check if HQ archive is continue
            chunk =  findChunk(m_catalogHi, prevEndTimeMs, DeviceFileCatalog::OnRecordHole_NextChunk);
            if (chunk.durationMs > 0 && chunk.startTimeMs + chunk.durationMs < prevEndTimeMs) 
            {
                // no HQ chunk found after LQ EOF
                chunk.startTimeMs = -1;
                return; 
            }

            // Switch to high quality, archive is continue
            chunkCatalog = m_catalogHi;
            if (chunk.startTimeMs < prevEndTimeMs)
                m_skipFramesToTime = prevEndTimeMs*1000;
            return;
        }
    }
    else if (m_currentChunk.durationMs == -1) {
        // It is not last chunk now. Update chunk duration
        m_currentChunk = findChunk(m_currentChunkCatalog, m_currentChunk.startTimeMs, DeviceFileCatalog::OnRecordHole_NextChunk);
        prevEndTimeMs = m_currentChunk.startTimeMs + m_currentChunk.durationMs;
    }

    chunk = findChunk(m_currentChunkCatalog, prevEndTimeMs, DeviceFileCatalog::OnRecordHole_NextChunk);
    chunkCatalog = m_currentChunkCatalog;

    if (chunk.startTimeMs == -1)
        return; // EOF reached. do not switch quality

    if (m_newQualityTmpData)
        return; // new quality data already ready. So, do not switch quality now.

    //if (m_currentChunk.durationMs == -1)
    //    return; // do not switch quality

    bool isCatalogEqualQuality = (m_quality == MEDIA_Quality_High && m_currentChunkCatalog == m_catalogHi) ||
        (m_quality == MEDIA_Quality_Low && m_currentChunkCatalog == m_catalogLow);

    // Check if quality switching required
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
                const DeviceFileCatalog::Chunk& chunkHi =  findChunk(m_catalogHi, prevEndTimeMs+SECOND_STREAM_FIND_EPS, DeviceFileCatalog::OnRecordHole_NextChunk);
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
        const DeviceFileCatalog::Chunk& chunkLow =  findChunk(m_catalogLow, chunk.startTimeMs, DeviceFileCatalog::OnRecordHole_NextChunk);
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
        const DeviceFileCatalog::Chunk& chunkHi =  findChunk(m_catalogHi, prevEndTimeMs, DeviceFileCatalog::OnRecordHole_NextChunk);
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
        int channel = data->channelNumber;
        if (!m_motionConnection[channel])
            m_motionConnection[channel] = QnMotionHelper::instance()->createConnection(m_resource, channel);
        QnMetaDataV1Ptr motion = m_motionConnection[channel]->getMotionData(data->timestamp);
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
    m_aviDelegate->setStorage(qnStorageMan->getStorageByUrl(url));
    bool rez = m_aviDelegate->open(m_fileRes);
    //if (rez)
    //    m_aviDelegate->setAudioChannel(m_selectedAudioChannel);
    return rez;
}

void QnServerArchiveDelegate::setSendMotion(bool value)
{
    m_sendMotion = value;
}

bool QnServerArchiveDelegate::setQuality(MediaQuality quality, bool fastSwitch)
{
    return setQualityInternal(quality, fastSwitch, m_lastPacketTime/1000 + 1, true);
}

bool QnServerArchiveDelegate::setQualityInternal(MediaQuality quality, bool fastSwitch, qint64 timeMs, bool recursive)
{
    m_quality = quality;
    m_newQualityTmpData.clear();
    m_newQualityAviDelegate.clear();

    if (!fastSwitch)
    {
        m_newQualityCatalog = quality == MEDIA_Quality_High ? m_catalogHi : m_catalogLow;
        m_newQualityChunk = findChunk(m_newQualityCatalog, timeMs, DeviceFileCatalog::OnRecordHole_NextChunk);
        if (m_newQualityChunk.distanceToTime(timeMs) > SECOND_STREAM_FIND_EPS) {

            return false;
        }
        QString url = m_newQualityCatalog->fullFileName(m_newQualityChunk);
        m_newQualityFileRes = QnAviResourcePtr(new QnAviResource(url));
        m_newQualityAviDelegate = QnAviArchiveDelegatePtr(new QnAviArchiveDelegate());
        m_newQualityAviDelegate->setUseAbsolutePos(false);

        m_newQualityAviDelegate->setStorage(qnStorageMan->getStorageByUrl(m_newQualityFileRes->getUrl()));
        if (!m_newQualityAviDelegate->open(m_newQualityFileRes))
            return false;
        qint64 chunkOffset = (timeMs - m_newQualityChunk.startTimeMs)*1000;
        m_newQualityAviDelegate->doNotFindStreamInfo();
        if (m_newQualityAviDelegate->seek(chunkOffset, false) == -1)
            return false;

        while (1) 
        {
            m_newQualityTmpData = m_newQualityAviDelegate->getNextData();
            if (m_newQualityTmpData == 0) 
            {
                qDebug() << "switching data not found. Chunk start=" << QDateTime::fromMSecsSinceEpoch(m_newQualityChunk.startTimeMs).toString();
                qDebug() << "requiredTime=" << QDateTime::fromMSecsSinceEpoch(timeMs).toString();
                // seems like requested position near chunk border. So, try next chunk
                if (recursive && m_newQualityChunk.startTimeMs != -1)
                    return setQualityInternal(quality, fastSwitch, m_newQualityChunk.startTimeMs+m_newQualityChunk.durationMs, false);
                break;
            }
            m_newQualityTmpData->timestamp += m_newQualityChunk.startTimeMs*1000;
            qDebug() << "switching data. skip time=" << QDateTime::fromMSecsSinceEpoch(m_newQualityTmpData->timestamp/1000).toString() << "flags=" << (m_newQualityTmpData->flags & AV_PKT_FLAG_KEY);
            if (m_newQualityTmpData->timestamp >= timeMs*1000ll && (m_newQualityTmpData->flags & AV_PKT_FLAG_KEY))
                break;
        }
    }

    return fastSwitch;
}
