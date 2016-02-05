#pragma once

#include <deque>
#include <atomic>

#include <nx/streaming/abstract_data_consumer.h>
#include <nx/streaming/video_data_packet.h>
#include <nx/streaming/audio_data_packet.h>

#include "media_fwd.h"

class QnArchiveStreamReader;

namespace nx {
namespace media {
class SeamlessVideoDecoder;

/*
* Private class used in nx::media::Player
*/
class PlayerDataConsumer : public QnAbstractDataConsumer
{
	Q_OBJECT
	typedef QnAbstractDataConsumer base_type;
public:
    PlayerDataConsumer(const std::unique_ptr<QnArchiveStreamReader>& archiveReader);
	virtual ~PlayerDataConsumer();

    QnVideoFramePtr dequeueVideoFrame();
    qint64 queueVideoDurationUsec() const;
signals:
    /** Hint to render to display current data with no delay due to seek operation in progress */
    void hurryUp();

    /** New video frame is decoded and ready for rendering */
	void gotVideoFrame();

    /** End of archive reached */
    void onEOF();
private slots:
    void onBeforeJump(qint64 timeUsec);
    void onJumpCanceled(qint64 timeUsec);
    void onJumpOccurred(qint64 timeUsec);
protected:
	virtual bool canAcceptData() const override;
	virtual bool processData(const QnAbstractDataPacketPtr& data) override;
    virtual void putData(const QnAbstractDataPacketPtr& data) override;
    
    /** Ask thread to stop. It's non blocking call. Thread will be stopped latter. */
    virtual void pleaseStop() override;
private:
    bool processEmptyFrame(const QnEmptyMediaDataPtr& data);
	bool processVideoFrame(const QnCompressedVideoDataPtr& data);
	bool processAudioFrame(const QnCompressedAudioDataPtr& data);

    void enqueueVideoFrame(QnVideoFramePtr decodedFrame);
    int getBufferingMask() const;
private:
	std::unique_ptr<SeamlessVideoDecoder> m_decoder;
			
    std::deque<QnVideoFramePtr> m_decodedVideo;
	QnWaitCondition m_queueWaitCond;
	QnMutex m_queueMutex;        //< sync with player thread
    QnMutex m_dataProviderMutex; //< sync with dataProvider thread

    int m_awaitJumpCounter; //< how many jump requests are queued
    int m_buffering;        //< reserved for future use for panoramic cameras
    int m_hurryUpToFrame;   //< display all data with no delay till this number
    std::atomic<qint64> m_lastMediaTimeUsec; //< UTC usec timestamp for the very last packet
            
    enum class NoDelayState
    {
        Disabled,        //< noDelay state isn't used
        Activated,       //< noDelay state is activated
        WaitForNextBOF   //< noDelay will be disabled as soon as next BOF frame is received
    };
    NoDelayState m_noDelayState;
};

}
}
