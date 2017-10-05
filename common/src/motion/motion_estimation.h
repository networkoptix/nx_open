#ifndef __MOTION_ESTIMATION_H__
#define __MOTION_ESTIMATION_H__

static const int MOTION_AGGREGATION_PERIOD = 300 * 1000;

#ifdef ENABLE_SOFTWARE_MOTION_DETECTION

#include <QtCore/QByteArray>
#include <nx/utils/thread/mutex.h>
#include "nx/streaming/media_data_packet.h"
#include "nx/streaming/video_data_packet.h"
#include "decoders/video/ffmpeg_video_decoder.h"
#include "core/resource/motion_window.h"

#include <motion/motion_detection.h>

static const int FRAMES_BUFFER_SIZE = 2;

class QnMotionEstimation
{
public:
    QnMotionEstimation();
    ~QnMotionEstimation();
    /*
    * Set motion mask as array of quint8 values in range [0..255] . array size is Qn::kMotionGridWidth * Qn::kMotionGridHeight
    * As motion data, motion mask is rotated to 90 degree.
    */
    void setMotionMask(const QnMotionRegion& region);
    void setChannelNum(int value);
#ifdef ENABLE_SOFTWARE_MOTION_DETECTION
    //void analizeFrame(const CLVideoDecoderOutput* frame);
    /*!
        \return true if successfully decoded and analyzed \a frame
    */
    bool analizeFrame(const QnCompressedVideoDataPtr& frame);
#endif
    QnAbstractCompressedMetadataPtr getMotion();
    bool existsMetadata() const;

    //!Returns resolution of video picture (it is known only after first successful \a QnMotionEstimation::analizeFrame call)
    QSize videoResolution() const;

private:
    void scaleMask(quint8* mask, quint8* scaledMask);
    void reallocateMask(int width, int height);
    void postFiltering();
    void analizeMotionAmount(quint8* frame);
	void scaleFrame(const uint8_t* data, int width, int height, int stride, uint8_t* frameBuffer,uint8_t* prevFrameBuffer, uint8_t* deltaBuffer);

private:
    QnMutex m_mutex;
    QnFfmpegVideoDecoder* m_decoder;
    QSharedPointer<CLVideoDecoderOutput> m_frames[2];

    quint8* m_motionMask;
    quint8* m_motionSensMask;
    quint8* m_scaledMask; // mask scaled to x * Qn::kMotionGridHeight. for internal use
    quint8* m_motionSensScaledMask;
    int m_scaledWidth;
    int m_xStep; // 8, 16, 24 e.t.c value
    int m_lastImgWidth;
    int m_lastImgHeight;
	uint8_t* m_frameDeltaBuffer;
    quint8* m_frameBuffer[FRAMES_BUFFER_SIZE];
    quint8* m_filteredFrame;
    quint32* m_resultMotion;
    qint64 m_firstFrameTime;
    qint64 m_lastFrameTime;
    int m_totalFrames;
    bool m_isNewMask;

    int m_linkedMap[Qn::kMotionGridWidth*Qn::kMotionGridHeight];
    int* m_linkedNums;
    int m_linkedSquare[Qn::kMotionGridWidth*Qn::kMotionGridHeight];
    //quint8 m_sadTransformMatrix[10][256];

    QSize m_videoResolution;

	//double m_sumLogTime;
	//int m_numFrame;
	int m_scaleXStep;
    int m_scaleYStep;
    int m_channelNum;
};

#endif  //ENABLE_SOFTWARE_MOTION_DETECTION

#endif // __MOTION_ESTIMATION_H__
