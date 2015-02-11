#include "archive_stream_reader.h"

#ifdef ENABLE_ARCHIVE

#include "stdint.h"

#include <core/resource/resource.h>

#include "utils/common/util.h"
#include "utils/media/externaltimesource.h"
#include "utils/common/synctime.h"


// used in reverse mode.
// seek by 1.5secs. It is prevents too fast seeks for short GOP, also some codecs has bagged seek function. Large step prevent seek
// forward instead seek backward
static const int MAX_KEY_FIND_INTERVAL = 10 * 1000 * 1000;

static const int FFMPEG_PROBE_BUFFER_SIZE = 1024 * 512;
static const qint64 LIVE_SEEK_OFFSET = 1000000ll * 10;

QnArchiveStreamReader::QnArchiveStreamReader(const QnResourcePtr& dev ) :
    QnAbstractArchiveReader(dev),
//protected
    m_currentTime(0),
    m_topIFrameTime(-1),
    m_bottomIFrameTime(-1),
    m_primaryVideoIdx(-1),
    m_audioStreamIndex(-1),
    mFirstTime(true),
    m_tmpSkipFramesToTime(AV_NOPTS_VALUE),
//private
    m_selectedAudioChannel(0),
    m_eof(false),
    m_reverseMode(false),
    m_prevReverseMode(false),
    m_frameTypeExtractor(0),
    m_lastGopSeekTime(-1),
    m_IFrameAfterJumpFound(false),
    m_requiredJumpTime(AV_NOPTS_VALUE),
    m_BOF(false),
    m_afterBOFCounter(-1),
    m_dataMarker(0),
    m_newDataMarker(0),
    m_currentTimeHint(AV_NOPTS_VALUE),
//private section 2
    m_bofReached(false),
    m_externalLocked(false),
    m_exactJumpToSpecifiedFrame(false),
    m_ignoreSkippingFrame(false),
    m_lastJumpTime(AV_NOPTS_VALUE),
    m_lastSkipTime(AV_NOPTS_VALUE),
    m_skipFramesToTime(0),
    m_keepLastSkkipingFrame(true),
    m_singleShot(false),
    m_singleQuantProcessed(false),
    m_nextData(0),
    m_quality(MEDIA_Quality_High),
    m_qualityFastSwitch(true),
    m_oldQuality(MEDIA_Quality_High),
    m_oldQualityFastSwitch(true),
    m_isStillImage(false),
    m_speed(1.0),
    m_pausedStart(false),
    m_sendMotion(false),
    m_prevSendMotion(false),
    m_outOfPlaybackMask(false),
    m_latPacketTime(DATETIME_NOW),
    m_stopCond(false)
{
    memset(&m_rewSecondaryStarted, 0, sizeof(m_rewSecondaryStarted));

    m_isStillImage = dev->hasFlags(Qn::still_image);

    if (dev->hasFlags(Qn::still_image) ||                           // disable cycle mode for images
       (
        (dev->hasFlags(Qn::utc) || dev->hasFlags(Qn::live))         // and for live non-local cameras
            && !dev->hasFlags(Qn::local))
       )      
        m_cycleMode = false;

    // Should init packets here as some times destroy (av_free_packet) could be called before init
    //connect(dev.data(), SIGNAL(statusChanged(Qn::ResourceStatus, Qn::ResourceStatus)), this, SLOT(onStatusChanged(Qn::ResourceStatus, Qn::ResourceStatus)));

}

/*
QnArchiveStreamReader::onStatusChanged(Qn::ResourceStatus oldStatus, Qn::ResourceStatus newStatus)
{
    if (newStatus == Qn::Offline)
        m_delegate->close();
}
*/

QnArchiveStreamReader::~QnArchiveStreamReader()
{
    stop();

    delete m_frameTypeExtractor;
    m_frameTypeExtractor = 0;
}

void QnArchiveStreamReader::nextFrame()
{
    if (m_navDelegate) {
        m_navDelegate->nextFrame();
        return;
    }
    emit nextFrameOccured();
    SCOPED_MUTEX_LOCK( lock, &m_jumpMtx);
    m_singleQuantProcessed = false;
    m_singleShowWaitCond.wakeAll();
}

void QnArchiveStreamReader::needMoreData()
{
    SCOPED_MUTEX_LOCK( lock, &m_jumpMtx);
    m_singleQuantProcessed = false;
    m_singleShowWaitCond.wakeAll();
}

void QnArchiveStreamReader::previousFrame(qint64 mksec)
{
    if (m_navDelegate) {
        m_navDelegate->previousFrame(mksec);
        return;
    }
    emit prevFrameOccured();
    jumpToPreviousFrame(mksec);
}

void QnArchiveStreamReader::resumeMedia()
{

    if (m_navDelegate) {
        m_navDelegate->resumeMedia();
        return;
    }
    if (m_singleShot)
    {
        emit streamResumed();
        m_delegate->setSingleshotMode(false);
        m_singleShot = false;
        //resumeDataProcessors();
        SCOPED_MUTEX_LOCK( lock, &m_jumpMtx);
        m_singleShowWaitCond.wakeAll();
    }
}

void QnArchiveStreamReader::pauseMedia()
{
    if (m_navDelegate) {
        m_navDelegate->pauseMedia();
        return;
    }
    if (!m_singleShot)
    {
        emit streamPaused();
        SCOPED_MUTEX_LOCK( lock, &m_jumpMtx);
        m_singleShot = true;
        m_singleQuantProcessed = true;
        m_lastSkipTime = m_lastJumpTime = AV_NOPTS_VALUE;
        m_delegate->setSingleshotMode(true);
    }
}

bool QnArchiveStreamReader::isMediaPaused() const
{
    if(m_navDelegate)
        return m_navDelegate->isMediaPaused();
    return m_singleShot || m_pausedStart;
}

void QnArchiveStreamReader::setCurrentTime(qint64 value)
{
    SCOPED_MUTEX_LOCK( mutex, &m_jumpMtx);
    m_currentTime = value;
}

qint64 QnArchiveStreamReader::currentTime() const
{
    SCOPED_MUTEX_LOCK( mutex, &m_jumpMtx);
    if (m_skipFramesToTime)
        return m_skipFramesToTime;
    else
        return m_currentTime;
}

AVIOContext* QnArchiveStreamReader::getIOContext()
{
    return 0;
}

QString QnArchiveStreamReader::serializeLayout(const QnResourceVideoLayout* layout)
{
    QString rez;
    QTextStream ost(&rez);
    ost << layout->size().width() << ',' << layout->size().height();
    for (int i = 0; i < layout->channelCount(); ++i)
        ost << ';' << layout->position(i).x() << ',' << layout->position(i).y();
    ost.flush();
    return rez;
}

QnConstResourceVideoLayoutPtr QnArchiveStreamReader::getDPVideoLayout() const
{
    if (!(m_delegate->getFlags() & QnAbstractArchiveDelegate::Flag_CanOfflineLayout))
        m_delegate->open(m_resource);
    return m_delegate->getVideoLayout();
}

QnConstResourceAudioLayoutPtr QnArchiveStreamReader::getDPAudioLayout() const
{
    if (!(m_delegate->getFlags() & QnAbstractArchiveDelegate::Flag_CanOfflineLayout))
        m_delegate->open(m_resource);
    return m_delegate->getAudioLayout();
}

bool QnArchiveStreamReader::init()
{
    setCurrentTime(0);

    m_jumpMtx.lock();
    qint64 requiredJumpTime = m_requiredJumpTime;
    MediaQuality quality = m_quality;
    m_jumpMtx.unlock();
    bool imSeek = m_delegate->getFlags() & QnAbstractArchiveDelegate::Flag_CanSeekImmediatly;
    if (imSeek && (requiredJumpTime != qint64(AV_NOPTS_VALUE) || m_reverseMode))
    {
        // It is optimization: open and jump at same time
        while (1)
        {
            m_delegate->setQuality(quality, true);
            qint64 jumpTime = requiredJumpTime != qint64(AV_NOPTS_VALUE) ? requiredJumpTime : qnSyncTime->currentUSecsSinceEpoch();
            bool seekOk = m_delegate->seek(jumpTime, true) >= 0;
            Q_UNUSED(seekOk)
            m_jumpMtx.lock();
            if (m_requiredJumpTime == requiredJumpTime) {
                if (requiredJumpTime != qint64(AV_NOPTS_VALUE))
                    m_requiredJumpTime = AV_NOPTS_VALUE;
                m_oldQuality = quality;
                m_oldQualityFastSwitch = true;
                m_jumpMtx.unlock();
                if (requiredJumpTime != qint64(AV_NOPTS_VALUE))
                    emit jumpOccured(requiredJumpTime);
                break;
            }
            requiredJumpTime = m_requiredJumpTime; // race condition. jump again
            quality = m_quality;
            m_jumpMtx.unlock();
        }
    }

    if (!m_delegate->open(m_resource)) {
        if (requiredJumpTime != qint64(AV_NOPTS_VALUE))
            emit jumpOccured(requiredJumpTime); 
        return false;
    }
    m_delegate->setAudioChannel(m_selectedAudioChannel);

    // Alloc common resources

    if (m_delegate->getFlags() & QnAbstractArchiveDelegate::Flag_SlowSource)
        emit slowSourceHint();

    return true;
}

bool QnArchiveStreamReader::offlineRangeSupported() const
{
    return m_delegate->getFlags() & QnAbstractArchiveDelegate::Flag_CanOfflineRange;
}

qint64 QnArchiveStreamReader::determineDisplayTime(bool reverseMode)
{
    QnlTimeSource* timeSource = 0;
    qint64 rez = AV_NOPTS_VALUE;
    for (int i = 0; i < m_dataprocessors.size(); ++i)
    {
        SCOPED_MUTEX_LOCK( mutex, &m_mutex);
        QnAbstractDataConsumer* dp = dynamic_cast<QnAbstractDataConsumer*>(m_dataprocessors.at(i));
        if( !dp )
            continue;
        if (dp->isRealTimeSource())
            return DATETIME_NOW;
        timeSource = dynamic_cast<QnlTimeSource*>(dp);
        if (timeSource) 
            break;
    }

    if (timeSource) {
        if (rez != qint64(AV_NOPTS_VALUE))
            rez = qMax(rez, timeSource->getExternalTime());
        else
            rez = timeSource->getExternalTime();
    }

    if(rez == qint64(AV_NOPTS_VALUE)) 
    {
        if (reverseMode)
            return endTime();
        else
            return startTime();
    }
    return rez;
}

bool QnArchiveStreamReader::getNextVideoPacket()
{
    while(1)
    {
        // Get next video packet and store it
        m_nextData = m_delegate->getNextData();
        if (!m_nextData)
            return false;

        if (m_nextData->dataType == QnAbstractMediaData::EMPTY_DATA)
            return false; // EOF/BOF reached
        if (m_nextData->dataType == QnAbstractMediaData::META_V1)
            m_skippedMetadata << m_nextData;

        QnCompressedVideoDataPtr video = qSharedPointerDynamicCast<QnCompressedVideoData>(m_nextData);
        if (video)
            return true;
    }
}

QnAbstractMediaDataPtr QnArchiveStreamReader::createEmptyPacket(bool isReverseMode)
{
    QnAbstractMediaDataPtr rez(new QnEmptyMediaData());
    rez->timestamp = isReverseMode ? 0 : DATETIME_NOW;
    if (m_BOF)
        rez->flags |= QnAbstractMediaData::MediaFlags_BOF;
    if (isReverseMode)
        rez->flags |= QnAbstractMediaData::MediaFlags_Reverse;
    rez->opaque = m_dataMarker;
    QnSleep::msleep(50);
    return rez;
}

void QnArchiveStreamReader::startPaused()
{
    m_pausedStart = true;
    m_singleQuantProcessed = true;
    start();
}

QnAbstractMediaDataPtr QnArchiveStreamReader::getNextData()
{
    while (!m_skippedMetadata.isEmpty())
        return m_skippedMetadata.dequeue();

    if (m_stopCond) {
        SCOPED_MUTEX_LOCK( lock, &m_stopMutex);
        m_delegate->close();
        while (m_stopCond && !needToStop())
            m_stopWaitCond.wait(&m_stopMutex);
        if (needToStop())
            return QnAbstractMediaDataPtr();
        m_delegate->seek(m_latPacketTime, true);
    }

    if (m_pausedStart)
    {
        m_pausedStart = false;
        SCOPED_MUTEX_LOCK( mutex, &m_jumpMtx);
        while (m_singleShot && m_singleQuantProcessed && !needToStop())
            m_singleShowWaitCond.wait(&m_jumpMtx);
    }

    //=================
    {
        SCOPED_MUTEX_LOCK( mutex, &m_jumpMtx);
        while (m_singleShot && m_skipFramesToTime == 0 && m_singleQuantProcessed && m_requiredJumpTime == qint64(AV_NOPTS_VALUE) && !needToStop())
            m_singleShowWaitCond.wait(&m_jumpMtx);
        //QnLongRunnable::pause();
    }

    bool singleShotMode = m_singleShot;

begin_label:
    if (needToStop())
        return QnAbstractMediaDataPtr();

    if (mFirstTime)
    {
        // this is here instead if constructor to unload ui thread
        m_BOF = true;
        if (init()) {
            mFirstTime = false;
        }
        else {
            // If media data can't be opened report 'no data'
            return createEmptyPacket(m_reverseMode);
        }
    }

    bool sendMotion = m_sendMotion;
    if (sendMotion != m_prevSendMotion) {
        m_delegate->setSendMotion(sendMotion);
        m_prevSendMotion = sendMotion;
    }

    int channelCount = m_delegate->getVideoLayout()->channelCount();

    m_jumpMtx.lock();
    bool reverseMode = m_reverseMode;
    qint64 jumpTime = m_requiredJumpTime;
    m_requiredJumpTime = AV_NOPTS_VALUE;
    MediaQuality quality = m_quality;
    bool qualityFastSwitch = m_qualityFastSwitch;
    qint64 tmpSkipFramesToTime = m_tmpSkipFramesToTime;
    m_tmpSkipFramesToTime = 0;
    bool exactJumpToSpecifiedFrame = m_exactJumpToSpecifiedFrame;
    qint64 currentTimeHint = m_currentTimeHint;
    m_currentTimeHint = AV_NOPTS_VALUE;

    bool needChangeQuality = m_oldQuality != quality || qualityFastSwitch > m_oldQualityFastSwitch;
    if (needChangeQuality) {
        m_oldQuality = quality;
        m_oldQualityFastSwitch = qualityFastSwitch;
    }

    m_dataMarker = m_newDataMarker;

    m_jumpMtx.unlock();

    // change quality checking
    if (needChangeQuality)
    {
        // !m_delegate->isRealTimeSource()
        bool needSeek = m_delegate->setQuality(quality, qualityFastSwitch);
        if (needSeek && jumpTime == qint64(AV_NOPTS_VALUE) && reverseMode == m_prevReverseMode)
        {
            qint64 displayTime = determineDisplayTime(reverseMode);
            if (displayTime != qint64(AV_NOPTS_VALUE)) {
                beforeJumpInternal(displayTime);
                if (!exactJumpToSpecifiedFrame && channelCount > 1)
                    setNeedKeyData();
                m_outOfPlaybackMask = false;
                internalJumpTo(displayTime);
                setSkipFramesToTime(displayTime, false);

                emit jumpOccured(displayTime);
                m_BOF = true;
            }
        }
    }


    // jump command
    if (jumpTime != qint64(AV_NOPTS_VALUE) && reverseMode == m_prevReverseMode) // if reverse mode is changing, ignore seek, because of reverseMode generate seek operation
    {
        m_outOfPlaybackMask = false;
        /*
        if (m_newDataMarker) {
            QString s;
            QTextStream str(&s);
            str << "setMarker=" << m_newDataMarker
                << " for Time=" << QDateTime::fromMSecsSinceEpoch(m_requiredJumpTime/1000).toString("hh:mm:ss.zzz");
            str.flush();
            NX_LOG(s, cl_logALWAYS);
        }
        */
        setSkipFramesToTime(tmpSkipFramesToTime, !exactJumpToSpecifiedFrame);
        m_ignoreSkippingFrame = exactJumpToSpecifiedFrame;
        if (!exactJumpToSpecifiedFrame && channelCount > 1)
            setNeedKeyData();
        internalJumpTo(jumpTime);
        emit jumpOccured(jumpTime);
        m_BOF = true;
    }

    // reverse mode changing
    bool delegateForNegativeSpeed = m_delegate->getFlags() & QnAbstractArchiveDelegate::Flag_CanProcessNegativeSpeed;
    if (reverseMode != m_prevReverseMode)
    {
        if (jumpTime != qint64(AV_NOPTS_VALUE))
            currentTimeHint = jumpTime;
        m_outOfPlaybackMask = false;
        m_bofReached = false;
        qint64 displayTime = currentTimeHint;
        if (currentTimeHint == DATETIME_NOW) {
            if (reverseMode)
                displayTime = endTime()-BACKWARD_SEEK_STEP;
        }
        else if (currentTimeHint == qint64(AV_NOPTS_VALUE))
        {
            if (jumpTime != qint64(AV_NOPTS_VALUE))
                displayTime = jumpTime;
            else
                displayTime = determineDisplayTime(reverseMode);

            //displayTime = jumpTime != qint64(AV_NOPTS_VALUE) ? jumpTime : determineDisplayTime(reverseMode);
        }

        m_delegate->onReverseMode(displayTime, reverseMode);
        m_prevReverseMode = reverseMode;
        if (!delegateForNegativeSpeed) {
            if (!exactJumpToSpecifiedFrame && channelCount > 1)
                setNeedKeyData();
            internalJumpTo(displayTime);
            if (reverseMode) {
                if (displayTime != DATETIME_NOW)
                    m_topIFrameTime = displayTime;
            }
            else
                setSkipFramesToTime(displayTime, false);
        }
        else {
            if (!reverseMode && displayTime != DATETIME_NOW && displayTime != qint64(AV_NOPTS_VALUE))
                setSkipFramesToTime(displayTime, false);
        }
        
        m_lastGopSeekTime = -1;
        m_BOF = true;
        m_afterBOFCounter = 0;
        if (jumpTime != qint64(AV_NOPTS_VALUE))
            emit jumpOccured(displayTime);
    }

    if (m_outOfPlaybackMask)
        return createEmptyPacket(reverseMode); // EOF reached

    if (m_afterMotionData)
    {
        QnAbstractMediaDataPtr result = m_afterMotionData;
        m_afterMotionData.clear();
        return result;
    }

    /*
    if (reverseMode && m_topIFrameTime > 0 && m_topIFrameTime <= m_delegate->startTime() && !m_cycleMode)
    {
        // BOF reached in reverse mode
        m_eof = true;
        return createEmptyPacket(reverseMode);
    }
    */

    if (m_delegate->startTime() == qint64(AV_NOPTS_VALUE))
        return createEmptyPacket(reverseMode); // no data at archive

    QnCompressedVideoDataPtr videoData;

    if (m_skipFramesToTime != 0)
        m_lastGopSeekTime = -1; // after user seek

    // If there is no nextPacket - read it from file, otherwise use saved packet
    if (m_nextData) {
        m_currentData = m_nextData;
        m_nextData.clear();
    }
    else {
        m_currentData = getNextPacket();
    }

    if (m_currentData == 0)
        return m_currentData;

    if (m_currentData->flags & QnAbstractMediaData::MediaFlags_Skip)
        goto begin_label;

    videoData = qSharedPointerDynamicCast<QnCompressedVideoData>(m_currentData);

    if (m_currentData->timestamp != qint64(AV_NOPTS_VALUE)) {
        setCurrentTime(m_currentData->timestamp);
    }


    if (videoData) // in case of video packet
    {
        if (reverseMode && !delegateForNegativeSpeed)
        {
            // I have found example where AV_PKT_FLAG_KEY detected very bad.
            // Same frame sometimes Key sometimes not. It is VC1 codec.
            // Manual detection for it stream better, but has artefacts too. I thinks some data lost in stream after jump
            // (after sequence header always P-frame, not I-Frame. But I returns I, because no I frames at all in other case)


            FrameTypeExtractor::FrameType frameType = FrameTypeExtractor::UnknownFrameType;
            if (videoData->context)
            {
                if (m_frameTypeExtractor == 0 || videoData->context->ctx() != m_frameTypeExtractor->getContext())
                {
                    delete m_frameTypeExtractor;
                    m_frameTypeExtractor = new FrameTypeExtractor((AVCodecContext*) videoData->context->ctx());
                }

                frameType = m_frameTypeExtractor->getFrameType((const quint8*) videoData->data(), videoData->dataSize());
            }
            bool isKeyFrame;

            if (frameType != FrameTypeExtractor::UnknownFrameType)
                isKeyFrame = frameType == FrameTypeExtractor::I_Frame;
            else {
                isKeyFrame =  m_currentData->flags  & AV_PKT_FLAG_KEY;
            }

            if (m_eof || (m_currentTime == 0 && m_bottomIFrameTime > 0 && m_topIFrameTime >= m_bottomIFrameTime))
            {
                // seek from EOF to BOF occured
                //Q_ASSERT(m_topIFrameTime != DATETIME_NOW);
                setCurrentTime(m_topIFrameTime);
                m_eof = false;
            }

            // Limitation for duration of the first GOP after reverse mode activation
            if (m_afterBOFCounter != -1)
            {
                if (m_afterBOFCounter == 0 && m_currentTime == INT64_MAX) 
                {
                    // no any packet yet readed from archive and eof reached. So, current time still unknown
                    QnSleep::msleep(10);
                    internalJumpTo(qnSyncTime->currentMSecsSinceEpoch()*1000 - BACKWARD_SEEK_STEP);
                    m_afterBOFCounter = 0;
                    goto begin_label;
                }

                m_afterBOFCounter++;
                if (m_afterBOFCounter >= MAX_FIRST_GOP_FRAMES) {
                    m_topIFrameTime = m_currentTime;
                    m_afterBOFCounter = -1;
                }
            }

            // multisensor cameras support
            int ch = videoData->channelNumber;
            if (ch > 0 && !m_rewSecondaryStarted[ch])
            {
                if (isKeyFrame) {
                    videoData->flags |= QnAbstractMediaData::MediaFlags_ReverseBlockStart;
                    m_rewSecondaryStarted[ch] = true;
                }
                else
                    goto begin_label; // skip 
            }

            if (isKeyFrame || m_currentTime >= m_topIFrameTime)
            {
                if (m_bottomIFrameTime == -1 && m_currentTime < m_topIFrameTime) 
                {
                    m_bottomIFrameTime = m_currentTime;
                    videoData->flags |= QnAbstractMediaData::MediaFlags_ReverseBlockStart;
                    memset(&m_rewSecondaryStarted, 0, sizeof(m_rewSecondaryStarted));
                }
                if (m_currentTime >= m_topIFrameTime)
                {
                    qint64 seekTime;
                    if (m_bofReached)
                    {
                        if (m_cycleMode)
                        {
                            if (m_delegate->endTime() != DATETIME_NOW)
                                seekTime = m_delegate->endTime() - BACKWARD_SEEK_STEP;
                            else
                                seekTime = qnSyncTime->currentMSecsSinceEpoch()*1000 - LIVE_SEEK_OFFSET;
                        }
                        else {
                            m_eof = true;
                            return createEmptyPacket(reverseMode);
                        }
                    }
                    else
                    {
                        // sometime av_file_ssek doesn't seek to key frame (seek direct to specified position)
                        // So, no KEY frame may be found after seek. At this case (m_bottomIFrameTime == -1) we increase seek interval
                        qint64 ct = m_currentTime != DATETIME_NOW ? m_currentTime-BACKWARD_SEEK_STEP : m_currentTime;
                        seekTime = m_bottomIFrameTime != -1 ? m_bottomIFrameTime : (m_lastGopSeekTime != -1 ? m_lastGopSeekTime : ct);
                        if (seekTime != DATETIME_NOW)
                            seekTime = qMax(m_delegate->startTime(), seekTime - BACKWARD_SEEK_STEP);
                        else
                            seekTime = qnSyncTime->currentMSecsSinceEpoch()*1000 - BACKWARD_SEEK_STEP;
                    }

                    if (m_currentTime != seekTime) {
                        m_currentData.clear();
                        qint64 tmpVal = m_bottomIFrameTime != -1 ? m_bottomIFrameTime : m_topIFrameTime;
                        internalJumpTo(seekTime);
                        m_bofReached = (seekTime == m_delegate->startTime()) || m_topIFrameTime > seekTime;
                        m_lastGopSeekTime = m_topIFrameTime; //seekTime;
                        //Q_ASSERT(m_lastGopSeekTime < DATETIME_NOW/2000ll);
                        m_topIFrameTime = tmpVal;
                        //return getNextData();
                        goto begin_label;
                    }
                    else {
                        m_bottomIFrameTime = m_currentTime;
                        m_topIFrameTime = m_currentTime + BACKWARD_SEEK_STEP;
                    }
                }
            }
            else if (m_bottomIFrameTime == -1) {
                // invalid seek. must be key frame
                m_currentData = QnAbstractMediaDataPtr();
                //return getNextData();
                goto begin_label;
            }
        }


        if (m_skipFramesToTime)
        {
            if (!m_nextData)
            {
                if (!getNextVideoPacket())
                {
                    // Some error or end of file. Stop reading frames.
                    setSkipFramesToTime(0, true);
                    QnAbstractMediaDataPtr tmp = m_nextData;
                    m_nextData.clear();
                    if (tmp && tmp->dataType == QnAbstractMediaData::EMPTY_DATA)
                    {
                        return tmp; //createEmptyPacket(reverseMode); // EOF/BOF reached
                    }
                }
            }

            if (m_nextData)
            {
                if (m_nextData->flags & QnAbstractMediaData::MediaFlags_LIVE)
                    setSkipFramesToTime(0, true);
                else if (!reverseMode && m_nextData->timestamp <= m_skipFramesToTime)
                    videoData->flags |= QnAbstractMediaData::MediaFlags_Ignore;
                else if (reverseMode && m_nextData->timestamp > m_skipFramesToTime)
                    videoData->flags |= QnAbstractMediaData::MediaFlags_Ignore;
                else {
                    if (!m_keepLastSkkipingFrame)
                        videoData->flags |= QnAbstractMediaData::MediaFlags_Ignore; // do not repeat last frame in such mode
                    else
                        videoData->flags &= ~(QnAbstractMediaData::MediaFlags_Ignore);
                    setSkipFramesToTime(0, true);
                }
            }
        }
    }
    else if (m_currentData->dataType == QnAbstractMediaData::EMPTY_DATA)
    {
        m_skipFramesToTime = 0;
    }

    if (videoData && (videoData->flags & QnAbstractMediaData::MediaFlags_Ignore) && m_ignoreSkippingFrame)
        goto begin_label;

    if (videoData && videoData->context) 
        m_codecContext = videoData->context;


    if (reverseMode && !delegateForNegativeSpeed)
        m_currentData->flags |= QnAbstractMediaData::MediaFlags_Reverse;

    if (videoData && singleShotMode && !(videoData->flags | QnAbstractMediaData::MediaFlags_Ignore)) {
        m_singleQuantProcessed = true;
        //m_currentData->flags |= QnAbstractMediaData::MediaFlags_SingleShot;
    }
    if (m_currentData)
        m_currentData->opaque = m_dataMarker;

    //if (m_currentData)
    //    qDebug() << "timestamp=" << QDateTime::fromMSecsSinceEpoch(m_currentData->timestamp/1000).toString("hh:mm:ss.zzz") << "flags=" << m_currentData->flags;


    // Do not display archive in a future
    if (!(m_delegate->getFlags() & QnAbstractArchiveDelegate::Flag_UnsyncTime)) 
    {
        if (videoData && !(videoData->flags & QnAbstractMediaData::MediaFlags_LIVE) && videoData->timestamp > qnSyncTime->currentUSecsSinceEpoch() && !reverseMode)
        {
            m_outOfPlaybackMask = true;
            return createEmptyPacket(reverseMode); // EOF reached
        }
    }

    // ensure Pos At playback mask
    if (!needToStop() && videoData && !(videoData->flags & QnAbstractMediaData::MediaFlags_Ignore) && !(videoData->flags & QnAbstractMediaData::MediaFlags_LIVE) 
        && m_nextData == 0) // check next data because of first current packet may be < required time (but next packet always > required time)
    {
        m_playbackMaskSync.lock();
        qint64 newTime = m_playbackMaskHelper.findTimeAtPlaybackMask(m_currentData->timestamp, !reverseMode);
        m_playbackMaskSync.unlock();

        qint64 maxTime = m_delegate->endTime();
        if (newTime == DATETIME_NOW || newTime == -1 || (maxTime != (qint64)AV_NOPTS_VALUE && newTime > maxTime)) {
            //internalJumpTo(qMax(0ll, newTime)); // seek to end or BOF.
            m_outOfPlaybackMask = true;
            return createEmptyPacket(reverseMode); // EOF reached
        }

        if (newTime != m_currentData->timestamp)
        {
            if (!exactJumpToSpecifiedFrame && channelCount > 1)
                setNeedKeyData();
            internalJumpTo(newTime);
            setSkipFramesToTime(newTime, true);
            m_eof = true;
            m_BOF = true;
            goto begin_label;
        }
    }

    if (m_currentData && m_currentData->dataType != QnAbstractMediaData::META_V1)
    {
        if (m_eof)
        {
            m_currentData->flags |= QnAbstractMediaData::MediaFlags_AfterEOF;
            m_eof = false;
        }
        if (m_BOF) {
            m_currentData->flags |= QnAbstractMediaData::MediaFlags_BOF;
            m_BOF = false;
        }
    }

    if (m_isStillImage)
        m_currentData->flags |= QnAbstractMediaData::MediaFlags_StillImage;

    SCOPED_MUTEX_LOCK( mutex, &m_jumpMtx);
    if (jumpTime != DATETIME_NOW)
        m_lastSkipTime = m_lastJumpTime = AV_NOPTS_VALUE; // allow duplicates jump to same position

    // process motion
    if (m_currentData && sendMotion)
    {
        int channel = m_currentData->channelNumber;
        if (!m_motionConnection[channel])
            m_motionConnection[channel] = m_delegate->getMotionConnection(channel);
        if (m_motionConnection[channel]) {
            QnMetaDataV1Ptr motion = m_motionConnection[channel]->getMotionData(m_currentData->timestamp);
            if (motion) 
            {
                motion->flags = m_currentData->flags;
                motion->opaque = m_currentData->opaque;
                m_afterMotionData = m_currentData;
                return motion;
            }
        }
    }
    if (m_currentData) 
        m_latPacketTime = (m_currentData->flags & QnAbstractMediaData::MediaFlags_LIVE) ? DATETIME_NOW : qMin(qnSyncTime->currentUSecsSinceEpoch(), m_currentData->timestamp);
    return m_currentData;
}

void QnArchiveStreamReader::internalJumpTo(qint64 mksec)
{
    m_skippedMetadata.clear();
    m_nextData.clear();
    m_afterMotionData.clear();
    qint64 seekRez = 0;
    if (mksec > 0) {
        seekRez = m_delegate->seek(mksec, !m_exactJumpToSpecifiedFrame);
    }
    else {
        // some files can't correctly jump to 0
        m_delegate->close();
        init();
        m_delegate->open(m_resource);
    }

    m_exactJumpToSpecifiedFrame = false;
    m_wakeup = true;
    m_bottomIFrameTime = -1;
    m_lastGopSeekTime = -1;
    m_topIFrameTime = seekRez != -1 ? seekRez : mksec;
    m_IFrameAfterJumpFound = false;
    m_eof = false;
    m_afterBOFCounter = -1;
    m_bofReached = false;
}

QnAbstractMediaDataPtr QnArchiveStreamReader::getNextPacket()
{
    QnAbstractMediaDataPtr result;
    while (!needToStop())
    {

        result = m_delegate->getNextData();

        if (result == 0 && !needToStop())
        {
            if (m_cycleMode)
            {
                if (m_delegate->endTime() < 1000 * 1000 * 5)
                    msleep(200); // prevent to fast file walk for very short files.
                m_delegate->close();
                m_skippedMetadata.clear();
                m_eof = true;

                if (!init())
                    return QnAbstractMediaDataPtr();
                result = m_delegate->getNextData();
                if (result == 0)
                    return result;
            }
            else
            {
                m_eof = true;
                return createEmptyPacket(isReverseMode());
            }
        }
        break;
    }

    return result;
}

unsigned int QnArchiveStreamReader::getCurrentAudioChannel() const
{
    return m_selectedAudioChannel;
}

QStringList QnArchiveStreamReader::getAudioTracksInfo() const
{
    QStringList rez;
    for (int i = 0; i < m_delegate->getAudioLayout()->channelCount(); ++i)
        rez << m_delegate->getAudioLayout()->getAudioTrackInfo(i).description;
    return rez;
}

bool QnArchiveStreamReader::setAudioChannel(unsigned int num)
{
    AVCodecContext* audioContext = m_delegate->setAudioChannel(num);
    if (audioContext)
        m_selectedAudioChannel = num;
    return audioContext != 0;
}

void QnArchiveStreamReader::setReverseMode(bool value, qint64 currentTimeHint)
{
    if (value != m_reverseMode) 
    {
        bool useMutex = !m_externalLocked;
        if (useMutex)
            m_jumpMtx.lock();
        m_lastSkipTime = m_lastJumpTime = AV_NOPTS_VALUE;
        m_reverseMode = value;
        m_currentTimeHint = currentTimeHint;
        if (useMutex)
            m_jumpMtx.unlock();
        m_delegate->beforeChangeReverseMode(m_reverseMode);
    }
}

bool QnArchiveStreamReader::isNegativeSpeedSupported() const
{
    return true; //!m_delegate->getVideoLayout() || m_delegate->getVideoLayout()->channelCount() == 1;
}

bool QnArchiveStreamReader::isSingleShotMode() const
{
    return m_singleShot;
}

void QnArchiveStreamReader::pleaseStop()
{
    QnAbstractArchiveReader::pleaseStop();
    if (m_delegate)
        m_delegate->beforeClose();
    m_singleShowWaitCond.wakeAll();
    m_stopWaitCond.wakeAll();
}

void QnArchiveStreamReader::setSkipFramesToTime(qint64 skipFramesToTime, bool keepLast)
{
    //SCOPED_MUTEX_LOCK( mutex, &m_jumpMtx);
    m_skipFramesToTime = skipFramesToTime;
    m_keepLastSkkipingFrame = keepLast;
}

bool QnArchiveStreamReader::isSkippingFrames() const
{
    SCOPED_MUTEX_LOCK( mutex, &m_jumpMtx);
    return m_skipFramesToTime != 0 || m_tmpSkipFramesToTime != 0;
}

void QnArchiveStreamReader::channeljumpToUnsync(qint64 mksec, int /*channel*/, qint64 skipTime)
{
    //qDebug() << "jumpTime=" << QDateTime::fromMSecsSinceEpoch(mksec/1000).toString("hh:mm:ss.zzz") << "skipTime=" << skipTime;
    m_singleQuantProcessed=false;
    //if (m_requiredJumpTime != AV_NOPTS_VALUE)
    //    emit jumpCanceled(m_requiredJumpTime);
    m_requiredJumpTime = mksec;
    m_tmpSkipFramesToTime = skipTime;
    m_singleShowWaitCond.wakeAll();
}

void QnArchiveStreamReader::directJumpToNonKeyFrame(qint64 mksec)
{
    if (mksec == qint64(AV_NOPTS_VALUE))
        return;

    if (m_navDelegate) {
        return m_navDelegate->directJumpToNonKeyFrame(mksec);
    }

    bool useMutex = !m_externalLocked;
    if (useMutex)
        m_jumpMtx.lock();
    
    beforeJumpInternal(mksec);
    m_exactJumpToSpecifiedFrame = true;
    channeljumpToUnsync(mksec, 0, mksec);

    if (useMutex)
        m_jumpMtx.unlock();
}

/*
void QnArchiveStreamReader::jumpWithMarker(qint64 mksec, bool findIFrame, int marker)
{
    bool useMutex = !m_externalLocked;
    if (useMutex)
        m_jumpMtx.lock();
    beforeJumpInternal(mksec);
    m_newDataMarker = marker;
    m_exactJumpToSpecifiedFrame = !findIFrame;
    channeljumpToUnsync(mksec, 0, 0);
    if (useMutex)
        m_jumpMtx.unlock();
}
*/

void QnArchiveStreamReader::setMarker(int marker)
{
    bool useMutex = !m_externalLocked;
    if (useMutex)
        m_jumpMtx.lock();
    m_newDataMarker = marker;
    if (useMutex)
        m_jumpMtx.unlock();
}

void QnArchiveStreamReader::setSkipFramesToTime(qint64 skipTime)
{
    if (m_navDelegate) {
        return m_navDelegate->setSkipFramesToTime(skipTime);
    }

    setSkipFramesToTime(skipTime, true);
    emit skipFramesTo(skipTime);

    if (isSingleShotMode())
        QnLongRunnable::resume();
}

bool QnArchiveStreamReader::jumpTo(qint64 mksec, qint64 skipTime)
{
    if (m_navDelegate) {
        return m_navDelegate->jumpTo(mksec, skipTime);
    }

    qint64 newTime = mksec;
    m_playbackMaskSync.lock();
    newTime = m_playbackMaskHelper.findTimeAtPlaybackMask(mksec, m_speed >= 0);
    m_playbackMaskSync.unlock();
    if (newTime != mksec)
        skipTime = 0;


    bool useMutex = !m_externalLocked;
    if (useMutex)
        m_jumpMtx.lock();

    bool needJump = newTime != m_lastJumpTime || m_lastSkipTime != skipTime;
    m_lastJumpTime = newTime;
    m_lastSkipTime = skipTime;

    if(useMutex)
        m_jumpMtx.unlock();

    if (needJump)
    {
        if (useMutex)
            m_jumpMtx.lock();
        beforeJumpInternal(newTime);
        channeljumpToUnsync(newTime, 0, skipTime);
        if (useMutex)
            m_jumpMtx.unlock();
    }

    //start(QThread::HighPriority);

    if (isSingleShotMode())
        QnLongRunnable::resume();
    return needJump;
}

void QnArchiveStreamReader::beforeJumpInternal(qint64 mksec)
{
    if (m_requiredJumpTime != qint64(AV_NOPTS_VALUE))
        emit jumpCanceled(m_requiredJumpTime);
    emit beforeJump(mksec);
    m_delegate->beforeSeek(mksec);
}

bool QnArchiveStreamReader::setSendMotion(bool value)
{
    if (m_delegate->getFlags() & QnAbstractArchiveDelegate::Flag_CanSendMotion)
    {
        m_sendMotion = value;
        return true;
    }
    else {
        return false;
    }
}


void QnArchiveStreamReader::setPlaybackRange(const QnTimePeriod& playbackRange)
{
    SCOPED_MUTEX_LOCK( lock, &m_playbackMaskSync);
    m_outOfPlaybackMask = false;
    m_playbackMaskHelper.setPlaybackRange(playbackRange);
}

QnTimePeriod QnArchiveStreamReader::getPlaybackRange() const
{
    return m_playbackMaskHelper.getPlaybackRange();
}

void QnArchiveStreamReader::setPlaybackMask(const QnTimePeriodList& playbackMask)
{
    SCOPED_MUTEX_LOCK( lock, &m_playbackMaskSync);
    m_outOfPlaybackMask = false;
    m_playbackMaskHelper.setPlaybackMask(playbackMask);
}

void QnArchiveStreamReader::onDelegateChangeQuality(MediaQuality quality)
{
    SCOPED_MUTEX_LOCK( lock, &m_jumpMtx);
    m_oldQuality = m_quality = quality;
    m_oldQualityFastSwitch = m_qualityFastSwitch = true;
}

void QnArchiveStreamReader::setQuality(MediaQuality quality, bool fastSwitch)
{
    if (m_quality != quality || fastSwitch > m_qualityFastSwitch)
    {
        bool useMutex = !m_externalLocked;
        if (useMutex)
            m_jumpMtx.lock();
        m_quality = quality;
        m_qualityFastSwitch = fastSwitch;
        if (useMutex)
            m_jumpMtx.unlock();
    }
}

MediaQuality QnArchiveStreamReader::getQuality() const
{
    return m_quality;
}

void QnArchiveStreamReader::lock()
{
    m_jumpMtx.lock();
    m_externalLocked = true; // external class locks mutex to perform atomic several params changing
}

void QnArchiveStreamReader::unlock()
{
    m_externalLocked = false;
    m_jumpMtx.unlock();
}


void QnArchiveStreamReader::setArchiveDelegate(QnAbstractArchiveDelegate* contextDelegate)
{
    m_delegate = contextDelegate;
    connect(m_delegate, SIGNAL(qualityChanged(MediaQuality)), this, SLOT(onDelegateChangeQuality(MediaQuality)), Qt::DirectConnection);
}

void QnArchiveStreamReader::setSpeed(double value, qint64 currentTimeHint)
{
    if (m_navDelegate) {
        m_navDelegate->setSpeed(value, currentTimeHint);
        return;
    }

    SCOPED_MUTEX_LOCK( mutex, &m_mutex);
    m_speed = value;
    for (int i = 0; i < m_dataprocessors.size(); ++i)
    {
        QnAbstractDataConsumer* dp = dynamic_cast<QnAbstractDataConsumer*>(m_dataprocessors.at(i));
        if( !dp )
            continue;
        dp->setSpeed(value);
    }
    setReverseMode(value < 0, currentTimeHint);
}

double QnArchiveStreamReader::getSpeed() const
{
    SCOPED_MUTEX_LOCK( mutex, &m_mutex);
    return m_speed;
}

QnMediaContextPtr QnArchiveStreamReader::getCodecContext() const
{
    return m_codecContext;
}

qint64 QnArchiveStreamReader::startTime() const
{
    Q_ASSERT(m_delegate);
    const QnTimePeriod p = m_playbackMaskHelper.getPlaybackRange();
    if (p.isEmpty())
        return m_delegate->startTime();
    else
        return p.startTimeMs*1000;
}

qint64 QnArchiveStreamReader::endTime() const
{
    Q_ASSERT(m_delegate);
    const QnTimePeriod p = m_playbackMaskHelper.getPlaybackRange();
    if (p.isEmpty())
        return m_delegate->endTime();
    else
        return p.endTimeMs()*1000;
}

void QnArchiveStreamReader::afterRun()
{
    if (m_delegate)
        m_delegate->close();
}

void QnArchiveStreamReader::setGroupId(const QByteArray& guid)
{
    if (m_delegate)
        m_delegate->setGroupId(guid);
}

void QnArchiveStreamReader::pause() 
{
    if (getResource()->hasParam(lit("groupplay"))) {
        SCOPED_MUTEX_LOCK( lock, &m_stopMutex);
        m_delegate->beforeClose();
        m_stopCond = true; // for VMAX
    }
    else {
        QnAbstractArchiveReader::pause();
    }
}

void QnArchiveStreamReader::resume() 
{
    if (getResource()->hasParam(lit("groupplay"))) {
        SCOPED_MUTEX_LOCK( lock, &m_stopMutex);
        m_stopCond = false; // for VMAX
        m_stopWaitCond.wakeAll();
    }
    else {
        QnAbstractArchiveReader::resume();
    }
}

bool QnArchiveStreamReader::isRealTimeSource() const
{
    return m_delegate && m_delegate->isRealTimeSource() && (m_requiredJumpTime == (qint64)AV_NOPTS_VALUE || m_requiredJumpTime == DATETIME_NOW);
}

#endif // ENABLE_ARCHIVE
