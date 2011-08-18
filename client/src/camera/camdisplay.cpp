#include "camdisplay.h"

#include "resource/media_resource.h"
#include "resourcecontrol/resource_pool.h"
#include "abstractrenderer.h"
#include "videostreamdisplay.h"
#include "audiostreamdisplay.h"

// a lot of small audio packets in bluray HD audio codecs. So, previous size 7 is not enought
#define CL_MAX_DISPLAY_QUEUE_SIZE 15
#define CL_MAX_DISPLAY_QUEUE_FOR_SLOW_SOURCE_SIZE 120
#define AUDIO_BUFF_SIZE (4000) // ms

static const qint64 MIN_VIDEO_DETECT_JUMP_INTERVAL = 100 * 1000; // 100ms
static const qint64 MIN_AUDIO_DETECT_JUMP_INTERVAL = MIN_VIDEO_DETECT_JUMP_INTERVAL + AUDIO_BUFF_SIZE*1000;
static const int MAX_VALID_SLEEP_TIME = 1000*1000*5;
static const int MAX_VALID_SLEEP_LIVE_TIME = 1000 * 500; // 5 seconds as most long sleep time

CLCamDisplay::CLCamDisplay()
    : QnAbstractDataConsumer(CL_MAX_DISPLAY_QUEUE_SIZE),
      m_delay(100*1000), // do not put a big value here to avoid cpu usage in case of zoom out
      m_playAudio(false),
      m_needChangePriority(false),
      m_hadAudio(false),
      m_lastAudioPacketTime(0),
      m_lastVideoPacketTime(0),
      m_lastDisplayedVideoTime(0),
      m_previousVideoTime(0),
      m_lastNonZerroDuration(0),
      m_previousVideoDisplayedTime(0),
      m_afterJump(false),
      m_displayLasts(0),
      m_ignoringVideo(false),
      m_isRealTimeSource(true),
      m_videoBufferOverflow(false),
      m_singleShotMode(false),
      m_singleShotQuantProcessed(false),
      m_jumpTime(0),
      m_playingCompress(0),
      m_playingBitrate(0)
{
    for (int i = 0; i< CL_MAX_CHANNELS; ++i)
        m_display[i] = 0;

    m_audioDisplay = new CLAudioStreamDisplay(AUDIO_BUFF_SIZE);
}

CLCamDisplay::~CLCamDisplay()
{
    for (int i = 0; i< CL_MAX_CHANNELS; ++i)
        delete m_display[i];

    clearVideoQueue();
    delete m_audioDisplay;
}

void CLCamDisplay::pause()
{
    QnAbstractDataConsumer::pause();

    m_audioDisplay->suspend();
}

void CLCamDisplay::resume()
{
    m_singleShotMode = false;
    m_audioDisplay->resume();

    QnAbstractDataConsumer::resume();
}

void CLCamDisplay::addVideoChannel(int index, CLAbstractRenderer *renderer, bool canDownscale)
{
    Q_ASSERT(index < CL_MAX_CHANNELS);

    m_display[index] = new CLVideoStreamDisplay(renderer, canDownscale);
}

void CLCamDisplay::display(QnCompressedVideoDataPtr vd, bool sleep)
{
    // simple data provider/streamer/streamreader has the same delay, but who cares ?
    // to avoid cpu usage in case of a lot data in queue(zoomed out on the scene, lets add same delays here )
    quint64 currentTime = vd->timestamp;

    // in ideal world data comes to queue at the same speed as it goes out
    // but timer on the sender side runs at a bit different rate in comparison to the timer here
    // adaptive delay will not solve all problems => need to minus little appendix based on queue size
    qint32 needToSleep = currentTime - m_previousVideoTime;
    if (m_isRealTimeSource)
    {
        needToSleep -= (m_dataQueue.size()) * 2 * 1000;
        if (needToSleep > MAX_VALID_SLEEP_LIVE_TIME)
            needToSleep = 0;
    }

    m_previousVideoTime = currentTime;

    //===== to avoid unrelated streams / stop play delays
    if (needToSleep < 0)
        needToSleep = 0;

    if (needToSleep > MAX_VALID_SLEEP_TIME) // in case of key frame only and sliding archive slider forward - would not look good; need to do smth
        needToSleep = 0;
    //=========

    if (sleep)
        m_delay.sleep(needToSleep);

    int channel = vd->channelNumber;

    CLVideoDecoderOutput::downscale_factor scaleFactor = CLVideoDecoderOutput::factor_any;
    if (channel > 0) // if device has more than one channel
    {
        // this here to avoid situation where different channels have different down scale factor; it might lead to diffrent size
        scaleFactor = m_display[0]->getCurrentDownscaleFactor(); // [0] - master channel
    }

    if (m_display[channel])
    {
        // sometimes draw + decoding takes a lot of time. so to be able always sync video and audio we MUST not draw
        QTime displayTime;
        displayTime.restart();

        bool draw = !vd->ignore && (sleep || (m_displayLasts * 1000 < needToSleep)); // do not draw if computer is very slow and we still wanna sync with audio

        // If there are multiple channels for this timestamp use only one of them
        if (channel == 0 && draw)
            m_previousVideoDisplayedTime = currentTime;

        CL_LOG(cl_logDEBUG2)
        {
            if (vd->ignore)
                cl_log.log(QLatin1String("Ignoring frame "), (int)vd->timestamp, cl_logDEBUG2);
            else
                cl_log.log(QLatin1String("Playing frame "), (int)vd->timestamp, cl_logDEBUG2);

            if (!draw)
                cl_log.log(QLatin1String("skip drawing frame!!"), displayTime.elapsed(), cl_logDEBUG2);

        }


        m_display[channel]->dispay(vd, draw, scaleFactor);

        if (!sleep)
            m_displayLasts = displayTime.elapsed(); // this is how long would i take to draw frame.

        //m_display[channel]->dispay(vd, sleep, scale_factor);
        //cl_log.log(" video queue size = ", m_videoQueue[0].size(),  cl_logALWAYS);
    }

    vd.clear();
}

void CLCamDisplay::jump(qint64 time)
{
    m_jumpTime = time;
    m_afterJump = true;
    clearUnprocessedData();
    m_singleShotMode = false;
}

void CLCamDisplay::setSingleShotMode(bool single)
{
    m_singleShotMode = single;
    m_singleShotQuantProcessed = false;
}

void CLCamDisplay::processData(QnAbstractDataPacketPtr data)
{
    QnAbstractMediaDataPacketPtr media = data.dynamicCast<QnAbstractMediaDataPacket>();
    Q_ASSERT(!media.isNull());

    QnCompressedVideoDataPtr vd;
    QnCompressedAudioDataPtr ad;
    if (media->dataType == QnAbstractMediaDataPacket::VIDEO)
    {
        vd = media.staticCast<QnCompressedVideoData>();
        m_ignoringVideo = vd->ignore;
    }
    else if (media->dataType == QnAbstractMediaDataPacket::AUDIO)
    {
        ad = media.staticCast<QnCompressedAudioData>();
    }

    bool flushCurrentBuffer = false;
    bool audioParamsChanged = ad && m_playingFormat != ad->format;
    if (media && ((media->flags & QnAbstractMediaDataPacket::MediaFlags_AfterEOF) || audioParamsChanged) &&
        m_videoQueue->size() > 0)
    {
        // skip data (play current buffer)
        flushCurrentBuffer = true;
    }

    if (m_afterJump)
    {
        qint64 ts = vd? vd->timestamp : ad->timestamp;
        // Some clips has very low key frame rate.
        // This condition protect audio buffer overflowing and improve seeking for such clips
        if (ad && ts < m_jumpTime - AUDIO_BUFF_SIZE / 2 * 1000)
            return; // skip packet

        m_afterJump = false;
        // clear everything we can
//            afterJump(ts);
    }

    if (ad && !flushCurrentBuffer)
    {
        if (audioParamsChanged)
        {
//                delete m_audioDisplay;
//                m_audioDisplay = new CLAudioStreamDisplay(AUDIO_BUFF_SIZE);
            m_playingFormat = ad->format;
        }

        // after seek, when audio is shifted related video (it is often),
        // first audio packet will be < seek threshold
        // so, second afterJump is generated after several video packets.
        // To prevent it, increase jump detection interval for audio

        if (ad->timestamp && ad->timestamp - m_lastAudioPacketTime < -MIN_AUDIO_DETECT_JUMP_INTERVAL)
        {
//                afterJump(ad->timestamp);
        }

        m_lastAudioPacketTime = ad->timestamp;

        // we synch video to the audio; so just put audio in player with out thinking
        if (m_playAudio)
        {
            if (m_audioDisplay->msInBuffer() > AUDIO_BUFF_SIZE)
            {
                // Audio buffer too large. waiting
                QnSleep::msleep(40);
            }

            m_audioDisplay->putData(ad, nextVideoImageTime(0));
            m_hadAudio = true;
        }
        else if (m_hadAudio)
        {
            m_audioDisplay->enqueueData(ad, nextVideoImageTime(0));
        }
    }

    if (vd || flushCurrentBuffer)
    {
        int channel = vd ? vd->channelNumber : 0;
        if (flushCurrentBuffer)
        {
            vd.clear();
        }
        else
        {
            if (vd->timestamp - m_lastVideoPacketTime < -MIN_VIDEO_DETECT_JUMP_INTERVAL)
            {
                afterJump(vd->timestamp);
            }
            m_lastVideoPacketTime = vd->timestamp;

            if (channel >= CL_MAX_CHANNELS)
                return;

            if (m_singleShotMode && m_singleShotQuantProcessed)
            {
                enqueueVideo(vd);
                return;
            }
        }

        // three are 3 possible scenarios:
        // 1) we do not have audio playing;
        if (!haveAudio())
        {
            qint64 m_videoDuration = m_videoQueue->size() * m_lastNonZerroDuration;
            if (vd && m_videoDuration > 1000 * 1000)
            {
                // skip current video packet, process it later
                vd.clear();
            }
            vd = nextInOutVideodata(vd, channel);
            if (!vd)
                return; // impossible? incoming vd != 0
            m_lastDisplayedVideoTime = vd->timestamp;
            display(vd, !vd->ignore);
            m_singleShotQuantProcessed = true;
            return;
        }

        // no more data expected; play as is
        if (flushCurrentBuffer)
            m_audioDisplay->playCurrentBuffer();

        // 2) we have audio and it's buffering (not playing yet)
        if (m_audioDisplay->isBuffering() && !flushCurrentBuffer)
        {
            // audio is not playing yet; video must not be played as well
            enqueueVideo(vd);
            return;
        }

        // 3) video and audio playing
        qint64 videoDuration;
        qint64 diff = diffBetweenVideoAndAudio(vd, channel, videoDuration);
        //cl_log.log("diff = ", (int)diff/1000, cl_logALWAYS);

        if (diff >= 2 * videoDuration && m_audioDisplay->msInBuffer() > 0) // factor 2 here is to avoid frequent switch between normal play and fast play
        {
            // video runs faster than audio; need to hold this video frame
            if (vd)
                enqueueVideo(vd);
            // avoid too fast buffer filling on startup
            qint64 sleepTime = qMin(diff, (m_audioDisplay->msInBuffer() - AUDIO_BUFF_SIZE / 2) * 1000ll);
            sleepTime = qMin(sleepTime, 500 * 1000ll);
            cl_log.log(QLatin1String("HOLD FRAME. sleep time="), sleepTime/1000.0, cl_logDEBUG1);
            if (sleepTime > 0)
                m_delay.sleep(sleepTime);
            return;
        }

        //if (diff < 2 * videoDuration) //factor 2 here is to avoid frequent switch between normal play and fast play
        else
        {
            // need to draw frame(s); at least one

            QnCompressedVideoDataPtr incoming;
            if (m_audioDisplay->msInBuffer() < AUDIO_BUFF_SIZE )
                incoming = vd; // process packet
            else {
                if (vd)
                    vd.clear(); // queue too large. postpone packet.
            }

            int fastFrames = 0;

            while(1)
            {
                //qint64 diff = diffBetweenVideoAndAudio(incoming, channel, videoDuration);

                vd = nextInOutVideodata(incoming, channel);
                incoming.clear();
                if (!vd)
                    break; // no more video in queue

                bool lastFrameToDisplay = diff > -2 * videoDuration; //factor 2 here is to avoid frequent switch between normal play and fast play

                //display(vd, lastFrameToDisplay && !vd->ignore && m_audioDisplay->msInBuffer() >= AUDIO_BUFF_SIZE / 10);
                m_lastDisplayedVideoTime = vd->timestamp;
                display(vd, lastFrameToDisplay && !vd->ignore);
                m_singleShotQuantProcessed = true;

                if (!lastFrameToDisplay)
                {
                    cl_log.log(QLatin1String("FAST PLAY, diff = "), (int)diff / 1000, cl_logDEBUG1);
                    //cl_log.log("ms audio buff = ", m_audioDisplay->msInBuffer(), cl_logWARNING);

                    ++fastFrames;

                    if (fastFrames == 2) // allow just 2 fast frames at the time
                        break;
                }

                if (lastFrameToDisplay)
                {
                    break;
                }
                diff = diffBetweenVideoAndAudio(incoming, channel, videoDuration);
            }
        }
    }
}

//==========================================================================

bool CLCamDisplay::haveAudio() const
{
    return (m_playAudio && m_hadAudio) && !m_ignoringVideo && m_audioDisplay->isFormatSupported();
}

QnCompressedVideoDataPtr CLCamDisplay::nextInOutVideodata(QnCompressedVideoDataPtr incoming, int channel)
{
    if (m_videoQueue[channel].isEmpty())
        return incoming;

    if (incoming)
        enqueueVideo(incoming);

    // queue is not empty
    return m_videoQueue[channel].dequeue();
}

quint64 CLCamDisplay::nextVideoImageTime(QnCompressedVideoDataPtr incoming, int channel) const
{
    if (m_videoQueue[channel].isEmpty())
        return incoming ? incoming->timestamp : 0;

    // queue is not empty
    return m_videoQueue[channel].head()->timestamp;
}

quint64 CLCamDisplay::nextVideoImageTime(int channel) const
{
    if (m_videoQueue[channel].isEmpty())
        return m_lastVideoPacketTime;
    if (m_videoBufferOverflow)
        return 0;
    return m_videoQueue[channel].head()->timestamp;
}

void CLCamDisplay::clearVideoQueue()
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        m_videoQueue[i].clear();
    m_videoBufferOverflow = false;
}

qint64 CLCamDisplay::diffBetweenVideoAndAudio(QnCompressedVideoDataPtr incoming, int channel, qint64& duration)
{
    qint64 currentPlayingVideoTime = nextVideoImageTime(incoming, channel);
    qint64 currentPlayingAudioTime = m_lastAudioPacketTime - (quint64)m_audioDisplay->msInBuffer()*1000;

    // strongly saning this duration of prev frame; not exact, but I think its fine
    // let's assume this frame has same duration
    duration = qMax(currentPlayingVideoTime - m_previousVideoTime, 0ll);
    if (duration == 0)
        duration = m_lastNonZerroDuration;
    else
        m_lastNonZerroDuration = duration;

    // difference between video and audio
    return currentPlayingVideoTime - currentPlayingAudioTime;
}

void CLCamDisplay::enqueueVideo(QnCompressedVideoDataPtr vd)
{
    m_videoQueue[vd->channelNumber].enqueue(vd);
    if (m_videoQueue[vd->channelNumber].size() > 60 * 6) // I assume we are not gonna buffer
    {
        cl_log.log(QLatin1String("Video buffer overflow!"), cl_logWARNING);
        (void)m_videoQueue->dequeue();
        // some protection for very large difference between video and audio tracks. Need to improve sync logic for this case (now a lot of glithces)
        m_videoBufferOverflow = true;
    }
}

void CLCamDisplay::afterJump(qint64 newTime)
{
    cl_log.log(QLatin1String("after jump"), cl_logWARNING);

    m_lastAudioPacketTime = newTime;
    m_lastVideoPacketTime = newTime;
    m_previousVideoTime = newTime;
    m_previousVideoDisplayedTime = 0;
    clearVideoQueue();
    m_audioDisplay->clearAudioBuffer();
}

void CLCamDisplay::setRealTimeStreamHint(bool value)
{
    m_isRealTimeSource = value;
}
