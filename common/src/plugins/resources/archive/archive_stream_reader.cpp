#include "archive_stream_reader.h"
#include "stdint.h"
#include "utils/media/frame_info.h"
#include "avi_files/avi_archive_delegate.h"
#include "utils/common/util.h"
#include "utils/media/externaltimesource.h"


// used in reverse mode.
// seek by 1.5secs. It is prevents too fast seeks for short GOP, also some codecs has bagged seek function. Large step prevent seek
// forward instead seek backward
static const int MAX_KEY_FIND_INTERVAL = 10 * 1000 * 1000;

static const int FFMPEG_PROBE_BUFFER_SIZE = 1024 * 512;
static const qint64 LIVE_SEEK_OFFSET = 1000000ll * 10;

QnArchiveStreamReader::QnArchiveStreamReader(QnResourcePtr dev ) :
    QnAbstractArchiveReader(dev),
    m_currentTime(0),
    m_primaryVideoIdx(-1),
    m_audioStreamIndex(-1),
    mFirstTime(true),
    m_eof(false),
    m_nextData(0),
    m_reverseMode(false),
    m_prevReverseMode(false),
    m_topIFrameTime(-1),
    m_bottomIFrameTime(-1),
    m_frameTypeExtractor(0),
    m_lastGopSeekTime(-1),
    m_IFrameAfterJumpFound(false),
    m_requiredJumpTime(AV_NOPTS_VALUE),
    m_lastFrameDuration(0),
    m_selectedAudioChannel(0),
    m_BOF(false),
    m_tmpSkipFramesToTime(AV_NOPTS_VALUE),
    m_afterBOFCounter(-1),
    m_singleShot(false),
    m_singleQuantProcessed(false),
    m_dataMarker(0),
    m_newDataMarker(0),
    m_skipFramesToTime(0),
    m_keepLastSkkipingFrame(true),
    m_ignoreSkippingFrame(false),
    m_lastJumpTime(AV_NOPTS_VALUE),
    m_exactJumpToSpecifiedFrame(false),
    m_quality(MEDIA_Quality_High),
    m_qualityFastSwitch(true),
    m_oldQualityFastSwitch(true),
    m_oldQuality(MEDIA_Quality_High),
    m_externalLocked(false),
    m_canChangeQuality(true),
    m_isStillImage(false),
    m_bofReached(false),
    m_currentTimeHint(-1),
    m_speed(1.0)
{
    m_isStillImage = dev->checkFlags(QnResource::still_image);
    // Should init packets here as some times destroy (av_free_packet) could be called before init
    //connect(dev.data(), SIGNAL(statusChanged(QnResource::Status, QnResource::Status)), this, SLOT(onStatusChanged(QnResource::Status, QnResource::Status)));

}

/*
QnArchiveStreamReader::onStatusChanged(QnResource::Status oldStatus, QnResource::Status newStatus)
{
    if (newStatus == QnResource::Offline)
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
    QMutexLocker lock(&m_jumpMtx);
    m_singleQuantProcessed = false;
    m_singleShowWaitCond.wakeAll();
}

void QnArchiveStreamReader::needMoreData()
{
    QMutexLocker lock(&m_jumpMtx);
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
    emit streamResumed();
    setSingleShotMode(false);
    resumeDataProcessors();
}

void QnArchiveStreamReader::pauseMedia()
{
    if (m_navDelegate) {
        m_navDelegate->pauseMedia();
        return;
    }
    emit streamPaused();
    QMutexLocker lock(&m_jumpMtx);
    m_singleShot = true;
    m_singleQuantProcessed = true;
    m_lastJumpTime = AV_NOPTS_VALUE;
}

bool QnArchiveStreamReader::isMediaPaused() const
{
    return m_singleShot && m_singleQuantProcessed;
}

void QnArchiveStreamReader::setCurrentTime(qint64 value)
{
    QMutexLocker mutex(&m_jumpMtx);
    m_currentTime = value;
}

qint64 QnArchiveStreamReader::currentTime() const
{
    QMutexLocker mutex(&m_jumpMtx);
    if (m_skipFramesToTime)
        return m_skipFramesToTime;
    else
        return m_currentTime;
}

ByteIOContext* QnArchiveStreamReader::getIOContext()
{
    return 0;
}

QString QnArchiveStreamReader::serializeLayout(const QnVideoResourceLayout* layout)
{
    QString rez;
    QTextStream ost(&rez);
    ost << layout->width() << ',' << layout->height();
    for (unsigned i = 0; i < layout->numberOfChannels(); ++i) {
        ost << ';' << layout->h_position(i) << ',' << layout->v_position(i);
    }
    ost.flush();
    return rez;
}

const QnVideoResourceLayout* QnArchiveStreamReader::getDPVideoLayout() const
{
    m_delegate->open(m_resource);
    return m_delegate->getVideoLayout();
}

const QnResourceAudioLayout* QnArchiveStreamReader::getDPAudioLayout() const
{
    m_delegate->open(m_resource);
    return m_delegate->getAudioLayout();
}

bool QnArchiveStreamReader::init()
{
    setCurrentTime(0);

    if (!m_delegate->open(m_resource))
        return false;
    m_delegate->setAudioChannel(m_selectedAudioChannel);
    m_channel_number = m_delegate->getVideoLayout()->numberOfChannels();

    // Alloc common resources
    m_lastFrameDuration = 0;

    if (m_delegate->getFlags() & QnAbstractArchiveDelegate::Flag_SlowSource)
        emit slowSourceHint();

    return true;
}

 qint64 QnArchiveStreamReader::determineDisplayTime(bool reverseMode)
 {
     qint64 rez = AV_NOPTS_VALUE;
     QMutexLocker mutex(&m_mutex);
     for (int i = 0; i < m_dataprocessors.size(); ++i)
     {
         QnAbstractDataConsumer* dp = m_dataprocessors.at(i);
         if (dp->isRealTimeSource())
             return DATETIME_NOW;
         QnlTimeSource* timeSource = dynamic_cast<QnlTimeSource*>(dp);
         if (timeSource) {
             if (rez != AV_NOPTS_VALUE)
                rez = qMax(rez, timeSource->getExternalTime());
             else
                 rez = timeSource->getExternalTime();
         }
     }
	 if(rez == AV_NOPTS_VALUE) 
     {
		//rez = m_currentTime;
         if (reverseMode)
             return m_delegate->endTime();
         else
             return m_delegate->startTime();
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
        if (video && video->channelNumber == 0)
            return true; // packet with primary video channel
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

QnAbstractMediaDataPtr QnArchiveStreamReader::getNextData()
{
    while (!m_skippedMetadata.isEmpty())
        return m_skippedMetadata.dequeue();

    //=================
    {
        QMutexLocker mutex(&m_jumpMtx);
        while (m_singleShot && m_skipFramesToTime == 0 && m_singleQuantProcessed && m_requiredJumpTime == AV_NOPTS_VALUE && !m_needStop)
            m_singleShowWaitCond.wait(&m_jumpMtx);
        //CLLongRunnable::pause();
    }

    bool singleShotMode = m_singleShot;

begin_label:
    if (!m_runing)
        return QnAbstractMediaDataPtr();

    if (mFirstTime)
    {
        // this is here instead if constructor to unload ui thread
        if (init()) {
            m_BOF = true;
            mFirstTime = false;
        }
        else {
            // If media data can't be opened wait 1 second and try again
            return QnAbstractMediaDataPtr();
        }
    }

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
    m_currentTimeHint = -1;
    m_jumpMtx.unlock();

    // change quality checking
    if (m_oldQuality != quality || qualityFastSwitch > m_oldQualityFastSwitch)
    {
        m_oldQuality = quality;
        m_oldQualityFastSwitch = qualityFastSwitch;
        // !m_delegate->isRealTimeSource()
        bool needSeek = m_delegate->setQuality(quality, qualityFastSwitch);
        if (needSeek && jumpTime == AV_NOPTS_VALUE && reverseMode == m_prevReverseMode)
        {
            qint64 displayTime = determineDisplayTime(reverseMode);
            beforeJumpInternal(displayTime);
            if (displayTime != AV_NOPTS_VALUE) {
                intChanneljumpTo(displayTime, 0);
                setSkipFramesToTime(displayTime, false);
                emit jumpOccured(displayTime);
                m_BOF = true;
            }
        }
    }


    // jump command
    if (jumpTime != AV_NOPTS_VALUE && reverseMode == m_prevReverseMode) // if reverse mode is changing, ignore seek, because of reverseMode generate seek operation
    {
        /*
        if (m_newDataMarker) {
            QString s;
            QTextStream str(&s);
            str << "setMarker=" << m_newDataMarker
                << " for Time=" << QDateTime::fromMSecsSinceEpoch(m_requiredJumpTime/1000).toString("hh:mm:ss.zzz");
            str.flush();
            cl_log.log(s, cl_logALWAYS);
        }
        */
        setSkipFramesToTime(tmpSkipFramesToTime, !exactJumpToSpecifiedFrame);
        m_ignoreSkippingFrame = exactJumpToSpecifiedFrame;
        intChanneljumpTo(jumpTime, 0);
        emit jumpOccured(jumpTime);
        m_BOF = true;
    }

    // reverse mode changing
    bool delegateForNegativeSpeed = m_delegate->getFlags() & QnAbstractArchiveDelegate::Flag_CanProcessNegativeSpeed;
    if (reverseMode != m_prevReverseMode)
    {
        m_bofReached = false;
        qint64 displayTime = currentTimeHint;
        if (currentTimeHint == -1)
            displayTime = jumpTime !=  AV_NOPTS_VALUE ? jumpTime : determineDisplayTime(reverseMode);

        m_delegate->onReverseMode(displayTime, reverseMode);
        m_prevReverseMode = reverseMode;
        if (!delegateForNegativeSpeed) {
            intChanneljumpTo(displayTime, 0);
            if (reverseMode) {
                if (displayTime != DATETIME_NOW)
                    m_topIFrameTime = displayTime;
            }
            else
                setSkipFramesToTime(displayTime, true);
        }
        m_lastGopSeekTime = -1;
        m_BOF = true;
        m_afterBOFCounter = 0;
        if (jumpTime != AV_NOPTS_VALUE)
            emit jumpOccured(displayTime);
    }
    m_dataMarker = m_newDataMarker;

    /*
    if (reverseMode && m_topIFrameTime > 0 && m_topIFrameTime <= m_delegate->startTime() && !m_cycleMode)
    {
        // BOF reached in reverse mode
        m_eof = true;
        return createEmptyPacket(reverseMode);
    }
    */

    if (m_delegate->startTime() == AV_NOPTS_VALUE)
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

    videoData = qSharedPointerDynamicCast<QnCompressedVideoData>(m_currentData);

    if (m_currentData->timestamp != AV_NOPTS_VALUE) {
        setCurrentTime(m_currentData->timestamp);
    }

    /*
    while (m_currentData->stream_index >= m_lastPacketTimes.size())
        m_lastPacketTimes << 0;
    if (m_currentData->timestamp == AV_NOPTS_VALUE) {
        m_lastPacketTimes[m_currentData->stream_index] += m_lastFrameDuration;
        m_currentTime = m_lastPacketTimes[m_currentData->stream_index];
    }
    else {
        m_currentTime =  m_currentData->timestamp;
        if (m_previousTime != -1 && m_currentTime != -1 && m_currentTime > m_previousTime && m_currentTime - m_previousTime < 100*1000)
            m_lastFrameDuration = m_currentTime - m_previousTime;
    }
    */

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

                frameType = m_frameTypeExtractor->getFrameType((quint8*) videoData->data.data(), videoData->data.size());
            }
            bool isKeyFrame;

            if (frameType != FrameTypeExtractor::UnknownFrameType)
                isKeyFrame = frameType == FrameTypeExtractor::I_Frame;
            else {
                isKeyFrame =  m_currentData->flags  & AV_PKT_FLAG_KEY;
            }

            if (m_eof || m_currentTime == 0 && m_bottomIFrameTime > 0 && m_topIFrameTime >= m_bottomIFrameTime)
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
                    intChanneljumpTo(QDateTime::currentMSecsSinceEpoch()*1000 - BACKWARD_SEEK_STEP, 0);
                    m_afterBOFCounter = 0;
                    goto begin_label;
                }

                m_afterBOFCounter++;
                if (m_afterBOFCounter >= MAX_FIRST_GOP_FRAMES) {
                    m_topIFrameTime = m_currentTime;
                    m_afterBOFCounter = -1;
                }
            }

            if (isKeyFrame || m_currentTime >= m_topIFrameTime)
            {
                if (m_bottomIFrameTime == -1 && m_currentTime < m_topIFrameTime) {
                    m_bottomIFrameTime = m_currentTime;
                    videoData->flags |= QnAbstractMediaData::MediaFlags_ReverseBlockStart;
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
                                seekTime = QDateTime::currentMSecsSinceEpoch()*1000 - LIVE_SEEK_OFFSET;
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
                            seekTime = QDateTime::currentMSecsSinceEpoch()*1000 - BACKWARD_SEEK_STEP;
                    }

                    if (m_currentTime != seekTime) {
                        m_currentData.clear();
                        qint64 tmpVal = m_bottomIFrameTime != -1 ? m_bottomIFrameTime : m_topIFrameTime;
                        intChanneljumpTo(seekTime, 0);
                        m_bofReached = seekTime == m_delegate->startTime();
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
            videoData->flags |= QnAbstractMediaData::MediaFlags_Reverse;
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
                        return createEmptyPacket(reverseMode); // EOF/BOF reached
                    }
                }
            }

            if (m_nextData)
            {
                if (m_nextData->flags & QnAbstractMediaData::MediaFlags_LIVE)
                    setSkipFramesToTime(0, true);
                else if (!reverseMode && m_nextData->timestamp < m_skipFramesToTime)
                    videoData->flags |= QnAbstractMediaData::MediaFlags_Ignore;
                else if (reverseMode && m_nextData->timestamp > m_skipFramesToTime)
                    videoData->flags |= QnAbstractMediaData::MediaFlags_Ignore;
                else {
                    if (!m_keepLastSkkipingFrame)
                        videoData->flags |= QnAbstractMediaData::MediaFlags_Ignore; // do not repeat last frame in such mode
                    setSkipFramesToTime(0, true);
                }
            }
        }
    }

    if (videoData && (videoData->flags & QnAbstractMediaData::MediaFlags_Ignore) && m_ignoreSkippingFrame)
        goto begin_label;


    if (m_currentData && m_eof)
    {
        m_currentData->flags |= QnAbstractMediaData::MediaFlags_AfterEOF;
        m_eof = false;
    }

    if (m_BOF) {
        m_currentData->flags |= QnAbstractMediaData::MediaFlags_BOF;
        m_BOF = false;
        //m_BOFTime = m_currentData->timestamp;
        /*
        QString msg;
        QTextStream str(&msg);
        str << "set BOF " << QDateTime::fromMSecsSinceEpoch(m_currentData->timestamp/1000).toString("hh:mm:ss.zzz") << " for marker " << m_dataMarker;
        str.flush();
        cl_log.log(msg, cl_logWARNING);
        */

    }

    if (m_currentData && singleShotMode && m_skipFramesToTime == 0) {
        m_singleQuantProcessed = true;
        //m_currentData->flags |= QnAbstractMediaData::MediaFlags_SingleShot;
    }
    if (m_currentData)
        m_currentData->opaque = m_dataMarker;

    //if (m_currentData)
    //    qDebug() << "timestamp=" << QDateTime::fromMSecsSinceEpoch(m_currentData->timestamp/1000).toString("hh:mm:ss.zzz") << "flags=" << m_currentData->flags;


    // ensure Pos At playback mask
    if (m_runing && videoData && !(videoData->flags & QnAbstractMediaData::MediaFlags_Ignore) && !(videoData->flags & QnAbstractMediaData::MediaFlags_LIVE) 
        && m_nextData == 0) // check next data because of first current packet may be < required time (but next packet always > required time)
    {
        m_playbackMaskSync.lock();
        qint64 newTime = m_playbackMaskHelper.findTimeAtPlaybackMask(m_currentData->timestamp, !reverseMode);
        m_playbackMaskSync.unlock();
        if (newTime != m_currentData->timestamp)
        {
            intChanneljumpTo(newTime, 0);
            setSkipFramesToTime(newTime, true);
            m_BOF = true;
            goto begin_label;
        }
    }

    if (m_isStillImage)
        m_currentData->flags |= QnAbstractMediaData::MediaFlags_StillImage;

    QMutexLocker mutex(&m_jumpMtx);
    if (m_lastJumpTime == DATETIME_NOW)
        m_lastJumpTime = AV_NOPTS_VALUE; // allow duplicates jump to live

    return m_currentData;
}

void QnArchiveStreamReader::intChanneljumpTo(qint64 mksec, int /*channel*/)
{
    m_skippedMetadata.clear();
    m_nextData.clear();
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
    while (!m_needStop)
    {
        result = m_delegate->getNextData();

        if (result == 0 && !m_needStop)
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
            else if (!m_eof)
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
    for (int i = 0; i < m_delegate->getAudioLayout()->numberOfChannels(); ++i)
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
        m_lastJumpTime = AV_NOPTS_VALUE;
        m_reverseMode = value;
        m_currentTimeHint = currentTimeHint;
        if (useMutex)
            m_jumpMtx.unlock();
        m_delegate->beforeChangeReverseMode(m_reverseMode);
    }
}

bool QnArchiveStreamReader::isNegativeSpeedSupported() const
{
    return !m_delegate->getVideoLayout() || m_delegate->getVideoLayout()->numberOfChannels() == 1;
}

void QnArchiveStreamReader::setSingleShotMode(bool single)
{
    if (m_navDelegate) {
        m_navDelegate->setSingleShotMode(single);
        return;
    }

    if (single == m_singleShot)
        return;
    m_delegate->setSingleshotMode(single);
    {
        QMutexLocker lock(&m_jumpMtx);
        m_singleShot = single;
        m_singleQuantProcessed = false;
        if (!m_singleShot)
            m_singleShowWaitCond.wakeAll();
    }
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
}

void QnArchiveStreamReader::setSkipFramesToTime(qint64 skipFramesToTime, bool keepLast)
{
    //QMutexLocker mutex(&m_jumpMtx);
    m_skipFramesToTime = skipFramesToTime;
    m_keepLastSkkipingFrame = keepLast;
}

bool QnArchiveStreamReader::isSkippingFrames() const
{
    QMutexLocker mutex(&m_jumpMtx);
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
    if (m_navDelegate) {
        return m_navDelegate->directJumpToNonKeyFrame(mksec);
    }

    QMutexLocker mutex(&m_jumpMtx);
    beforeJumpInternal(mksec);
    m_exactJumpToSpecifiedFrame = true;
    channeljumpToUnsync(mksec, 0, mksec);
}

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


    m_jumpMtx.lock();
    bool needJump = newTime != m_lastJumpTime;
    m_lastJumpTime = newTime;
    m_jumpMtx.unlock();

    if (needJump)
    {
        QMutexLocker mutex(&m_jumpMtx);
		beforeJumpInternal(newTime);
        channeljumpToUnsync(newTime, 0, skipTime);
    }

    if (isSingleShotMode())
        CLLongRunnable::resume();
    return needJump;
}

void QnArchiveStreamReader::beforeJumpInternal(qint64 mksec)
{
	if (m_requiredJumpTime != AV_NOPTS_VALUE)
		emit jumpCanceled(m_requiredJumpTime);
	emit beforeJump(mksec);
	m_delegate->beforeSeek(mksec);
}

bool QnArchiveStreamReader::setSendMotion(bool value)
{
    /*
    if (m_navDelegate) {
        return m_navDelegate->setSendMotion(value);
    }
    */
    QnAbstractFilterPlaybackDelegate* maskedDelegate = dynamic_cast<QnAbstractFilterPlaybackDelegate*>(m_delegate);
    if (maskedDelegate) {
        maskedDelegate->setSendMotion(value);
        if (!mFirstTime && !m_delegate->isRealTimeSource())
            jumpToPreviousFrame(determineDisplayTime(m_reverseMode));
        return true;
    }
    else {
        return false;
    }
}

bool QnArchiveStreamReader::setMotionRegion(const QRegion& region)
{
    /*
    if (m_navDelegate) {
        return m_navDelegate->setMotionRegion(region);
    }
    */
    QnAbstractFilterPlaybackDelegate* maskedDelegate = dynamic_cast<QnAbstractFilterPlaybackDelegate*>(m_delegate);
    if (maskedDelegate) {
        maskedDelegate->setMotionRegion(region);
        if (!mFirstTime && !m_delegate->isRealTimeSource())
            jumpToPreviousFrame(determineDisplayTime(m_reverseMode));
        return true;
    }
    else {
        return false;
    }
}

void QnArchiveStreamReader::setPlaybackMask(const QnTimePeriodList& playbackMask)
{
    QMutexLocker lock(&m_playbackMaskSync);
    m_playbackMaskHelper.setPlaybackMask(playbackMask);
}

void QnArchiveStreamReader::disableQualityChange()
{
    m_canChangeQuality = false;
}

void QnArchiveStreamReader::enableQualityChange()
{
    m_canChangeQuality = true;
}


void QnArchiveStreamReader::onDelegateChangeQuality(MediaQuality quality)
{
    QMutexLocker lock(&m_jumpMtx);
    m_quality = quality;
    m_qualityFastSwitch = true;
}

void QnArchiveStreamReader::setQuality(MediaQuality quality, bool fastSwitch)
{
    if (m_canChangeQuality && (m_quality != quality || fastSwitch > m_qualityFastSwitch)) 
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

bool QnArchiveStreamReader::getQuality() const
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

    QMutexLocker mutex(&m_mutex);
    m_speed = value;
    for (int i = 0; i < m_dataprocessors.size(); ++i)
    {
        QnAbstractDataConsumer* dp = m_dataprocessors.at(i);
        dp->setSpeed(value);
    }
    setReverseMode(value < 0, currentTimeHint);
}

double QnArchiveStreamReader::getSpeed() const
{
    QMutexLocker mutex(&m_mutex);
    return m_speed;
}

