#ifndef clcam_display_h_1211
#define clcam_display_h_1211

#include <QtCore/QMutex>

#include "core/dataconsumer/dataconsumer.h"
#include "core/datapacket/mediadatapacket.h"
#include "core/resource/media_resource_layout.h" // for CL_MAX_CHANNELS
#include "utils/common/adaptivesleep.h"

class CLAbstractRenderer;
class CLVideoStreamDisplay;
class CLAudioStreamDisplay;

class CLCamDisplay : public QnAbstractDataConsumer
{
    Q_OBJECT

public:
    CLCamDisplay();
    virtual ~CLCamDisplay();

    void pause();
    void resume();

    void addVideoChannel(int index, CLAbstractRenderer *renderer, bool canDownscale);

    void setSingleShotMode(bool single);

    void setRealTimeStreamHint(bool value);

protected:
    void processData(QnAbstractDataPacketPtr data);

private:
    void display(QnCompressedVideoDataPtr vd, bool sleep);
    void playAudio(bool play);

    bool haveAudio() const;

    // schedule to clean up buffers all;
    // schedule - coz I do not want to introduce mutexes
    //I assume that first incoming frame after jump is keyframe
    void jump(qint64 time);

    // puts in in queue and returns first in queue
    QnCompressedVideoDataPtr nextInOutVideodata(QnCompressedVideoDataPtr incoming, int channel);

    // this function doest not changes any quues; it just returns time of next frame been displayed
    quint64 nextVideoImageTime(QnCompressedVideoDataPtr incoming, int channel) const;

    quint64 nextVideoImageTime(int channel) const;

    // this function returns diff between video and audio at any given moment
    qint64 diffBetweenVideoAndAudio(QnCompressedVideoDataPtr incoming, int channel, qint64& duration);

    void clearVideoQueue();
    void enqueueVideo(QnCompressedVideoDataPtr vd);
    void afterJump(qint64 new_time);

private:
    QQueue<QnCompressedVideoDataPtr> m_videoQueue[CL_MAX_CHANNELS];

    CLVideoStreamDisplay* m_display[CL_MAX_CHANNELS];
    CLAudioStreamDisplay* m_audioDisplay;

    CLAdaptiveSleep m_delay;

    bool m_playAudioSet;
    bool m_playAudio;
    bool m_needChangePriority;

    /**
      * got at least one audio packet
      */
    bool m_hadAudio;

    qint64 m_lastAudioPacketTime;
    qint64 m_lastVideoPacketTime;
    qint64 m_lastDisplayedVideoTime;

    qint64 m_previousVideoTime;
    quint64 m_lastNonZerroDuration;
    quint64 m_previousVideoDisplayedTime;

    bool m_afterJump;

    int m_displayLasts;

    bool m_ignoringVideo;

    bool m_isRealTimeSource;
    QAudioFormat m_expectedAudioFormat;
    QMutex m_audioChangeMutex;
    bool m_videoBufferOverflow;
    bool m_singleShotMode;
    bool m_singleShotQuantProcessed;
    qint64 m_jumpTime;
    CLCodecAudioFormat m_playingFormat;
    int m_playingCompress;
    int m_playingBitrate;
};

#endif //clcam_display_h_1211
