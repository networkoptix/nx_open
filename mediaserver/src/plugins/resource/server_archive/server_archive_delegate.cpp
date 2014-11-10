
#include "server_archive_delegate.h"

#include <QtCore/QMutexLocker>

#include <server/server_globals.h>

#include "core/datapacket/video_data_packet.h"
#include "core/resource_management/resource_pool.h"
#include "utils/common/util.h"
#include "motion/motion_archive.h"
#include "motion/motion_helper.h"
#include "utils/common/sleep.h"
#include "core/resource/network_resource.h"
#include "plugins/resource/avi/avi_resource.h"

static const qint64 MOTION_LOAD_STEP = 1000ll * 3600;
static const int SECOND_STREAM_FIND_EPS = 1000 * 5;
static const int USEC_IN_MSEC = 1000;

QnServerArchiveDelegate::QnServerArchiveDelegate(): 
    QnAbstractArchiveDelegate(),
    m_opened(false),
    m_lastPacketTime(0),
    m_skipFramesToTime(0),
    m_reverseMode(false),
    m_selectedAudioChannel(0),
    m_lastSeekTime(AV_NOPTS_VALUE),
    m_afterSeek(false),
    //m_sendMotion(false),
    m_eof(false),
    m_quality(MEDIA_Quality_High),
    m_mutex( QMutex::Recursive )    //just to be sure no callback can occur and block
{
    m_flags |= Flag_CanSendMotion;
    m_aviDelegate = QnAviArchiveDelegatePtr(new QnAviArchiveDelegate());
    m_aviDelegate->setUseAbsolutePos(false);
    m_aviDelegate->setFastStreamFind(true);

    m_newQualityAviDelegate = QnAviArchiveDelegatePtr(0);
}

QnServerArchiveDelegate::~QnServerArchiveDelegate()
{
}

qint64 QnServerArchiveDelegate::startTime()
{
    QMutexLocker lk( &m_mutex );

    if (m_catalogHi && m_catalogHi->minTime() != (qint64)AV_NOPTS_VALUE)
    {
        if (m_catalogLow && m_catalogLow->minTime() != (qint64)AV_NOPTS_VALUE)
            return qMin(m_catalogHi->minTime(), m_catalogLow->minTime())*1000;
        else
            return m_catalogHi->minTime()*1000;
    }
    else if (m_catalogLow && m_catalogLow->minTime() != (qint64)AV_NOPTS_VALUE)
    {
        return m_catalogLow->minTime()*1000;
    }
    else
        return AV_NOPTS_VALUE;
}

qint64 QnServerArchiveDelegate::endTime()
{
    QMutexLocker lk( &m_mutex );

    qint64 timeHi = m_catalogHi ? m_catalogHi->maxTime() : AV_NOPTS_VALUE;
    qint64 timeLow = m_catalogLow ? m_catalogLow->maxTime() : AV_NOPTS_VALUE;

    qint64 rez;
    if (timeHi != (qint64)AV_NOPTS_VALUE && timeLow != (qint64)AV_NOPTS_VALUE)
        rez = qMax(timeHi, timeLow);
    else if (timeHi != (qint64)AV_NOPTS_VALUE)
        rez = timeHi;
    else
        rez = timeLow;

    if (rez != (qint64)AV_NOPTS_VALUE && rez != DATETIME_NOW)
        rez *= 1000;
    
    return rez;
}

bool QnServerArchiveDelegate::isOpened() const
{
    return m_opened;
}

bool QnServerArchiveDelegate::open(const QnResourcePtr &resource)
{
    QMutexLocker lk( &m_mutex );

    if (m_opened)
        return true;
    m_resource = resource;
    QnNetworkResourcePtr netResource = qSharedPointerDynamicCast<QnNetworkResource>(resource);
    Q_ASSERT(netResource != 0);
    m_dialQualityHelper.setResource(netResource);

    m_catalogHi = qnStorageMan->getFileCatalog(netResource->getUniqueId(), QnServer::HiQualityCatalog);
    m_catalogLow = qnStorageMan->getFileCatalog(netResource->getUniqueId(), QnServer::LowQualityCatalog);

    m_currentChunkCatalog = m_quality == MEDIA_Quality_Low ? m_catalogLow : m_catalogHi;
    m_opened = true;
    return true;
}

void QnServerArchiveDelegate::close()
{
    QMutexLocker lk( &m_mutex );

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

    DeviceFileCatalog::Chunk newChunk;
    DeviceFileCatalogPtr newChunkCatalog;

    DeviceFileCatalog::FindMethod findMethod = m_reverseMode ? DeviceFileCatalog::OnRecordHole_PrevChunk : DeviceFileCatalog::OnRecordHole_NextChunk;
    bool isePrecSeek = !m_reverseMode &&  m_quality == MEDIA_Quality_High; // do not try short LQ chunk if ForcedHigh quality and do not try short HQ chunk for LQ quality
    m_dialQualityHelper.findDataForTime(timeMs, newChunk, newChunkCatalog, findMethod, isePrecSeek); // use precise find if no REW mode
    m_currentChunkInfo.startTimeUsec = newChunk.startTimeMs * USEC_IN_MSEC;
    m_currentChunkInfo.durationUsec = newChunk.durationMs * USEC_IN_MSEC;
    if (!m_reverseMode && newChunk.endTimeMs() < timeMs)
    {
        m_eof = true;
        return time;
    }


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

    if (newChunk.startTimeMs != m_currentChunk.startTimeMs || newChunkCatalog != m_currentChunkCatalog)
    {
        //bool isStreamsFound = m_aviDelegate->isStreamsFound() && newChunkCatalog == m_currentChunkCatalog;
        if (!switchToChunk(newChunk, newChunkCatalog))
            return -1;
        //if (isStreamsFound)
        //    m_aviDelegate->doNotFindStreamInfo(); // optimization
    }


    qint64 seekRez = m_aviDelegate->seek(chunkOffset, findIFrame);
    if (seekRez == -1)
        return seekRez;
    qint64 rez = m_currentChunk.startTimeMs*1000 + seekRez;
    //NX_LOG("jump time ", t.elapsed(), cl_logALWAYS);
    /*
    QString s;
    QTextStream str(&s);
    str << "server seek:" << QDateTime::fromMSecsSinceEpoch(time/1000).toString("hh:mm:ss.zzz") << " time=" << t.elapsed();
    str.flush();
    NX_LOG(s, cl_logALWAYS);
    */
    m_lastSeekTime = rez;
    m_afterSeek = true;
    return rez;
}

qint64 QnServerArchiveDelegate::seek(qint64 time, bool findIFrame)
{
    QMutexLocker lk( &m_mutex );

    //m_tmpData.clear();
    // change time by playback mask
    m_eof = false;
    if (!m_motionRegion.isEmpty()) 
    {
        //time = correctTimeByMask(time, m_reverseMode);
        m_eof = time == (qint64)AV_NOPTS_VALUE;
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

bool QnServerArchiveDelegate::getNextChunk(DeviceFileCatalog::Chunk& chunk, DeviceFileCatalogPtr& chunkCatalog)
{
    QMutexLocker lk( &m_mutex );

    if (m_currentChunk.durationMs == -1)
        m_currentChunkCatalog->updateChunkDuration(m_currentChunk); // may be opened chunk already closed. Update duration if needed
    if (m_currentChunk.durationMs == -1) {
        if (!m_reverseMode)
            m_eof = true;
        return false;
    }
    m_skipFramesToTime = m_currentChunk.endTimeMs()*1000;
    m_dialQualityHelper.findDataForTime(m_currentChunk.endTimeMs(), chunk, chunkCatalog, DeviceFileCatalog::OnRecordHole_NextChunk, false);
    return chunk.startTimeMs > m_currentChunk.startTimeMs || chunk.endTimeMs() > m_currentChunk.endTimeMs();
}

QnAbstractMediaDataPtr QnServerArchiveDelegate::getNextData()
{
    QMutexLocker lk( &m_mutex );

    if (m_eof)
        return QnAbstractMediaDataPtr();

    /*
    if (m_tmpData)
    {
        QnAbstractMediaDataPtr rez = m_tmpData;
        m_tmpData.clear();
        return rez;
    }
    */

    //int waitMotionCnt = 0;
begin_label:
    QnAbstractMediaDataPtr data = m_aviDelegate->getNextData();
    int chunkSwitchCnt = 0;
    while (!data || (m_currentChunk.durationMs != -1 && data->timestamp >= m_currentChunk.durationMs*1000))
    {
        DeviceFileCatalog::Chunk chunk;
        DeviceFileCatalogPtr chunkCatalog;
        do {
            if (!getNextChunk(chunk, chunkCatalog))
            {
                if (m_reverseMode) {
                    data = QnAbstractMediaDataPtr(new QnWritableCompressedVideoData(CL_MEDIA_ALIGNMENT, 0));
                    data->timestamp = INT64_MAX; // EOF reached
                }
                else if (data)
                    data->timestamp +=m_currentChunk.startTimeMs*1000;
                return data;
            }
        } while (!switchToChunk(chunk, chunkCatalog));

        if (m_skipFramesToTime > m_currentChunk.startTimeMs*1000)
        {
            qint64 chunkOffset = m_skipFramesToTime - m_currentChunk.startTimeMs*1000;
            m_aviDelegate->seek(chunkOffset, true);
        }

        data = m_aviDelegate->getNextData();
        if (data) {
            data->flags &= ~QnAbstractMediaData::MediaFlags_BOF;
        }
        else {
            if (++chunkSwitchCnt > 10)
                break;
        }
    }

    if (data && !(data->flags & QnAbstractMediaData::MediaFlags_LIVE)) 
    {
        data->timestamp +=m_currentChunk.startTimeMs*1000;
        m_lastPacketTime = data->timestamp;

        if (m_skipFramesToTime) {
            if (data->timestamp < m_skipFramesToTime) 
            {
                if (data->dataType == QnAbstractMediaData::AUDIO)
                    goto begin_label; // skip data
                data->flags |= QnAbstractMediaData::MediaFlags_Ignore;
                data->timestamp = m_skipFramesToTime;
            }
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
    
    /*
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
    */
    return data;
}

QnAbstractMotionArchiveConnectionPtr QnServerArchiveDelegate::getMotionConnection(int channel)
{
    QMutexLocker lk( &m_mutex );

    return QnMotionHelper::instance()->createConnection(m_resource, channel);
}

QnAbstractArchiveDelegate::ArchiveChunkInfo QnServerArchiveDelegate::getLastUsedChunkInfo() const
{
    return m_currentChunkInfo;
}

QnResourceVideoLayoutPtr QnServerArchiveDelegate::getVideoLayout()
{
    QMutexLocker lk( &m_mutex );

    return m_aviDelegate->getVideoLayout();
}

QnResourceAudioLayoutPtr QnServerArchiveDelegate::getAudioLayout()
{
    QMutexLocker lk( &m_mutex );

    return m_aviDelegate->getAudioLayout();
}

void QnServerArchiveDelegate::onReverseMode(qint64 displayTime, bool value)
{
    QMutexLocker lk( &m_mutex );

    m_reverseMode = value;
    m_aviDelegate->onReverseMode(displayTime, value);
}

AVCodecContext* QnServerArchiveDelegate::setAudioChannel(int num)
{
    QMutexLocker lk( &m_mutex );

    m_selectedAudioChannel = num;
    return m_aviDelegate->setAudioChannel(num);
}

bool QnServerArchiveDelegate::switchToChunk(const DeviceFileCatalog::Chunk newChunk, const DeviceFileCatalogPtr& newCatalog)
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
    if (rez)
        m_aviDelegate->setAudioChannel(m_selectedAudioChannel);
    return rez;
}

/*
void QnServerArchiveDelegate::setSendMotion(bool value)
{
    m_sendMotion = value;
}
*/

bool QnServerArchiveDelegate::setQuality(MediaQuality quality, bool fastSwitch)
{
    QMutexLocker lk( &m_mutex );

    return setQualityInternal(quality, fastSwitch, m_lastPacketTime/1000 + 1, true);
}

bool QnServerArchiveDelegate::setQualityInternal(MediaQuality quality, bool fastSwitch, qint64 timeMs, bool recursive)
{
    m_dialQualityHelper.setPrefferedQuality(quality);
    m_quality = quality;
    m_newQualityTmpData.clear();
    m_newQualityAviDelegate.clear();

    if (!fastSwitch)
    {
        // no immediate seek is need. change catalog on next i-frame
        

        m_newQualityCatalog = (quality == MEDIA_Quality_Low ? m_catalogLow : m_catalogHi);
        m_newQualityChunk = findChunk(m_newQualityCatalog, timeMs, DeviceFileCatalog::OnRecordHole_NextChunk);
        if (m_newQualityChunk.startTimeMs == -1)
            return false; // requested quality is absent at all

        if (m_newQualityCatalog == m_currentChunkCatalog) 
        {
            // we already on requested quality
            if (m_currentChunk.startTimeMs == m_newQualityChunk.startTimeMs)
                m_currentChunk.durationMs = m_newQualityChunk.durationMs; // also remove current chunk duration limit if exists
            return false; // no seek is required
        }

        if (m_newQualityChunk.startTimeMs >= m_currentChunk.endTimeMs())
            return false; // requested quality absent for current position. Current chunk can be played to the end. no seek is needed (return false)

        
        QString url = m_newQualityCatalog->fullFileName(m_newQualityChunk);
        m_newQualityFileRes = QnAviResourcePtr(new QnAviResource(url));
        m_newQualityAviDelegate = QnAviArchiveDelegatePtr(new QnAviArchiveDelegate());
        m_newQualityAviDelegate->setUseAbsolutePos(false);
        m_newQualityAviDelegate->setFastStreamFind(true);

        m_newQualityAviDelegate->setStorage(qnStorageMan->getStorageByUrl(m_newQualityFileRes->getUrl()));
        if (!m_newQualityAviDelegate->open(m_newQualityFileRes))
            return false;
        m_newQualityAviDelegate->setAudioChannel(m_selectedAudioChannel);
        qint64 chunkOffset = (timeMs - m_newQualityChunk.startTimeMs)*1000;
        //m_newQualityAviDelegate->doNotFindStreamInfo();
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
    return fastSwitch; // if fastSwitch return true that mean need seek
}
