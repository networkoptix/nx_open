#include "archive_stream_reader.h"
#include "stdint.h"
#include <libavformat/avformat.h>
#include "utils/media/frame_info.h"


QMutex QnArchiveStreamReader::avi_mutex;
QSemaphore QnArchiveStreamReader::aviSemaphore(4);

// used in reverse mode.
// seek by 1.5secs. It is prevents too fast seeks for short GOP, also some codecs has bagged seek function. Large step prevent seek
// forward instead seek backward
static const int MAX_KEY_FIND_INTERVAL = 10 * 1000 * 1000;
static const qint64 BACKWARD_SEEK_STEP =  2000 * 1000; 

class QnAviSemaphoreHelpr
{
public:
    QnAviSemaphoreHelpr(QSemaphore& sem):
      m_sem(sem)
      {
          m_sem.tryAcquire();
      }

    ~QnAviSemaphoreHelpr()
    {
        m_sem.release();
    }

private:
    QSemaphore& m_sem;

};



static const int FFMPEG_PROBE_BUFFER_SIZE = 1024 * 512;

QnArchiveStreamReader::QnArchiveStreamReader(QnResource* dev ) :
    QnAbstractArchiveReader(dev),
    m_currentTime(0),
    m_previousTime(-1),
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
    m_lastUIJumpTime(AV_NOPTS_VALUE),
    m_lastFrameDuration(0),
    m_selectedAudioChannel(0)
{
    // Should init packets here as some times destroy (av_free_packet) could be called before init
}

QnArchiveStreamReader::~QnArchiveStreamReader()
{
    delete m_frameTypeExtractor;
    m_frameTypeExtractor = 0;
}

void QnArchiveStreamReader::previousFrame(qint64 mksec)
{
    jumpToPreviousFrame(mksec, true);
    QnAbstractArchiveReader::previousFrame(mksec);
}

qint64 QnArchiveStreamReader::currentTime() const
{
    QMutexLocker mutex(&m_cs);

    qint64 jumpTime = skipFramesToTime();

	if (jumpTime)
		return jumpTime;

	return m_currentTime;
}

ByteIOContext* QnArchiveStreamReader::getIOContext()
{
	return 0;
}

QString QnArchiveStreamReader::serializeLayout(const QnDeviceVideoLayout* layout)
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

QnDeviceVideoLayout* QnArchiveStreamReader::getVideoLayout()
{
    m_delegate->open(m_resource);
    return m_delegate->getVideoLayout();
}

QnDeviceAudioLayout* QnArchiveStreamReader::getAudioLayout()
{
    m_delegate->open(m_resource);
    return m_delegate->getAudioLayout();
}

bool QnArchiveStreamReader::init()
{
	QMutexLocker mutex(&m_cs);

	m_currentTime = 0;
	m_previousTime = -1;

    if (!m_delegate->open(m_resource))
        return false;
    m_delegate->setAudioChannel(m_selectedAudioChannel);
    m_channel_number = m_delegate->getVideoLayout()->numberOfChannels();

    m_lengthMksec = contentLength();
    if (m_lengthMksec == AV_NOPTS_VALUE)
        m_lengthMksec = 0;

    // Alloc common resources
    m_lastFrameDuration = 0;

    emit realTimeStreamHint(false);
    if (m_delegate->getFlags() & QnAbstractArchiveDelegate::Flag_SlowSource)
        emit slowSourceHint();

    return true;
}

 qint64 QnArchiveStreamReader::determineDisplayTime()
 {
     qint64 rez = 0;
     QMutexLocker mutex(&m_proc_CS);
     for (int i = 0; i < m_dataprocessors.size(); ++i)
     {
         QnAbstractDataConsumer* dp = m_dataprocessors.at(i);
         rez = qMax(rez, dp->currentTime());
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

        QnCompressedVideoDataPtr video = qSharedPointerDynamicCast<QnCompressedVideoData>(m_nextData);
        if (video && video->channelNumber == 0)
            return true; // packet with primary video channel
    }
}

QnAbstractMediaDataPtr QnArchiveStreamReader::getNextData()
{
begin_label:
    bool BOF = false;
    {
        QMutexLocker mutex(&m_jumpMtx);
        if (m_requiredJumpTime != AV_NOPTS_VALUE)
        {
            m_lastGopSeekTime = -1;
            intChanneljumpTo(m_requiredJumpTime, 0);
            m_lastUIJumpTime = m_requiredJumpTime;
            m_requiredJumpTime = AV_NOPTS_VALUE;
            BOF = true;
        }
    }
	if (mFirstTime)
	{
		QMutexLocker mutex(&avi_mutex); // speeds up concurrent reading of lots of files, if files not cashed yet
        // this is here instead if constructor to unload ui thread
		if (init()) 
		    mFirstTime = false;
        else
            return QnAbstractMediaDataPtr();
	}

    bool reverseMode = m_reverseMode;
    bool delegateForNegativeSpeed = m_delegate->getFlags() & QnAbstractArchiveDelegate::Flag_CanProcessNegativeSpeed;
    if (reverseMode != m_prevReverseMode)
    {
        qint64 displayTime = determineDisplayTime();
        m_delegate->onReverseMode(displayTime, reverseMode);
        m_prevReverseMode = reverseMode;
        if (!delegateForNegativeSpeed) {
            m_lastGopSeekTime = -1;
            intChanneljumpTo(displayTime, 0);
            if (reverseMode)
                m_topIFrameTime = displayTime;
            else
                setSkipFramesToTime(displayTime);
        }
    }

	QnAviSemaphoreHelpr sem(aviSemaphore);

    QnCompressedVideoDataPtr videoData;
	{
		QMutexLocker mutex(&m_cs);
        if (skipFramesToTime() != 0)
            m_lastGopSeekTime = -1; // after user seek

        if (m_nextData && m_previousTime == -1)
        {
            m_nextData = QnAbstractMediaDataPtr();
        }

		// If there is no nextPacket - read it from file, otherwise use saved packet
        if (m_nextData) {
            m_currentData = m_nextData;
            m_nextData = QnAbstractMediaDataPtr();
        }
        else {
            m_currentData = getNextPacket();
        }

        if (m_currentData == 0)
            return m_currentData;

        videoData = qSharedPointerDynamicCast<QnCompressedVideoData>(m_currentData);
        m_previousTime = m_currentTime;

        if (m_currentData->timestamp != AV_NOPTS_VALUE) 
            m_currentTime =  m_currentData->timestamp;

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
                if (m_frameTypeExtractor == 0 || videoData->context != m_frameTypeExtractor->getContext()) 
                {
                    delete m_frameTypeExtractor;
                    m_frameTypeExtractor = new FrameTypeExtractor((AVCodecContext*) videoData->context);
                }

                frameType = m_frameTypeExtractor->getFrameType((quint8*) videoData->data.data(), videoData->data.size());
            }
            bool isKeyFrame;
            
            if (frameType != FrameTypeExtractor::UnknownFrameType)
                isKeyFrame = frameType == FrameTypeExtractor::I_Frame;
            else {
                isKeyFrame =  m_currentData->flags  & AV_PKT_FLAG_KEY;
            }
            
            if (m_eof || m_currentTime == 0 && m_bottomIFrameTime > AV_REVERSE_BLOCK_START && m_topIFrameTime >= m_bottomIFrameTime) {
                // seek from EOF to BOF occured
                m_currentTime = m_topIFrameTime;
                m_eof = false;
            }

            if (isKeyFrame || m_currentTime >= m_topIFrameTime)
            {
                if (m_bottomIFrameTime == -1 && m_currentTime < m_topIFrameTime) {
                    m_bottomIFrameTime = m_currentTime;
                    videoData->flags |= AV_REVERSE_BLOCK_START;
                }
                if (m_currentTime >= m_topIFrameTime)
                {
                    // sometime av_file_ssek doesn't seek to key frame (seek direct to specified position)
                    // So, no KEY frame may be found after seek. At this case (m_bottomIFrameTime == -1) we increase seek interval
                    qint64 seekTime = m_bottomIFrameTime != -1 ? m_bottomIFrameTime : (m_lastGopSeekTime != -1 ? m_lastGopSeekTime : m_currentTime-BACKWARD_SEEK_STEP);
                    seekTime = qMax(0ll, seekTime - BACKWARD_SEEK_STEP);
                    if (m_currentTime != seekTime) {
                        m_currentData = QnAbstractMediaDataPtr();
                        qint64 tmpVal = m_bottomIFrameTime != -1 ? m_bottomIFrameTime : m_topIFrameTime;
                        if (m_lastGopSeekTime == 0) {
                            seekTime = m_lengthMksec - BACKWARD_SEEK_STEP;
                            tmpVal = m_lengthMksec;
                        }
                        intChanneljumpTo(seekTime, 0);
                        m_lastGopSeekTime = seekTime;
                        m_topIFrameTime = tmpVal;
                        //return getNextData();
                        if (m_runing)
                            goto begin_label;
                        else
                            return QnAbstractMediaDataPtr();
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
                if (m_runing)
                    goto begin_label;
                else
                    return QnAbstractMediaDataPtr();
            }
            videoData->flags |= AV_REVERSE_PACKET;
        }

        m_useTwice = false;

        /*
        // -------------------------------------------
        // workaround ffmpeg bugged seek
        // todo: move it code to AVI delegate
        if (videoData->channelNumber == 0) 
        {
            if (m_lastUIJumpTime != AV_NOPTS_VALUE)
            {
                QMutexLocker mutex(&m_cs);
                if (m_lastUIJumpTime - m_currentTime > MAX_KEY_FIND_INTERVAL) 
                {
                    // can't find I-frame after jump. jump again and disable I-frame searcher
                    m_currentData = QnAbstractMediaDataPtr();
                    intChanneljumpTo(m_lastUIJumpTime, 0);
                    m_lastUIJumpTime = AV_NOPTS_VALUE;
                    goto begin_label;
                }
                else  
                {
                    if (videoData->flags & AV_PKT_FLAG_KEY) {
                        m_IFrameAfterJumpFound = true;
                        m_lastUIJumpTime = AV_NOPTS_VALUE;
                    }
                    if (!reverseMode && !m_IFrameAfterJumpFound)
                    {
                        m_currentData = QnAbstractMediaDataPtr();
                        if (m_currentTime >= m_topIFrameTime) // m_topIFrameTime used as jump time
                            intChanneljumpTo(m_topIFrameTime - BACKWARD_SEEK_STEP, 0);
                        if (m_runing)
                            goto begin_label;
                        else
                            return QnAbstractMediaDataPtr();
                    }
                }
            }
        }
        */

        // -------------------------------------------

		if (skipFramesToTime())
		{
			if (!m_nextData)
			{
				QMutexLocker mutex(&m_cs);

                if (!getNextVideoPacket())
                {
                    // Some error or end of file. Stop reading frames.
                    setSkipFramesToTime(0);

                    m_nextData = QnAbstractMediaDataPtr();
                }
			}

			if (m_nextData)
			{
				if (!reverseMode && m_nextData->timestamp < skipFramesToTime())
					videoData->ignore = true;
                else if (reverseMode && m_nextData->timestamp > skipFramesToTime())
                    videoData->ignore = true;
                else {
					setSkipFramesToTime(0);
                }
			}
		}

		//=================
		if (isSingleShotMode() && skipFramesToTime() == 0)
			CLLongRunnable::pause();
	}

	if (m_currentData && m_eof)
	{
		m_currentData->flags |= QnAbstractMediaData::MediaFlags_AfterEOF;
		m_eof = false;
	}
    if (BOF)
        m_currentData->flags |= QnAbstractMediaData::MediaFlags_BOF;        

	return m_currentData;
}

void QnArchiveStreamReader::channeljumpTo(qint64 mksec, int channel)
{
    QMutexLocker mutex(&m_jumpMtx);
    m_requiredJumpTime = mksec;
}

void QnArchiveStreamReader::intChanneljumpTo(qint64 mksec, int /*channel*/)
{
    QMutexLocker mutex(&m_cs);
    if (mksec > 0) {
        m_delegate->seek(mksec);
    }
    else {
        // some files can't correctly jump to 0
        m_delegate->close();
        init();
        m_delegate->open(m_resource);
    }

	m_previousTime = -1;
	m_wakeup = true;
    m_bottomIFrameTime = -1;
    m_topIFrameTime = mksec;
    m_IFrameAfterJumpFound = false;
    m_eof = false;
}

QnAbstractMediaDataPtr QnArchiveStreamReader::getNextPacket()
{
    QnAbstractMediaDataPtr result;
	while (1)
	{
		result = m_delegate->getNextData();

		if (result == 0 && m_cycleMode)
		{
            if (m_lengthMksec < 1000 * 1000 * 5)
                msleep(200); // prevent to fast file walk for very short files.
            m_delegate->close();
            m_eof = true;

            if (!init())
                return QnAbstractMediaDataPtr();
            result = m_delegate->getNextData();
            if (result == 0)
                return result;
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
    if (audioContext) {
        m_selectedAudioChannel = num;
        emit audioParamsChanged(audioContext);
    }
    return audioContext != 0;
}

void QnArchiveStreamReader::setReverseMode(bool value)
{
    m_reverseMode = value;
}

bool QnArchiveStreamReader::isNegativeSpeedSupported() const
{
    return !m_delegate->getVideoLayout() || m_delegate->getVideoLayout()->numberOfChannels() == 1;
}
