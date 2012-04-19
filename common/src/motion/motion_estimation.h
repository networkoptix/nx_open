#ifndef __MOTION_ESTIMATION_H__
#define __MOTION_ESTIMATION_H__

#include "utils/media/frame_info.h"
#include "core/datapacket/mediadatapacket.h"
#include "decoders/video/ffmpeg.h"

class QnMotionEstimation
{
public:
    QnMotionEstimation();
    ~QnMotionEstimation();
    /*
    * Set motion mask as array of quint8 values in range [0..255] . array size is MD_WIDTH * MD_HEIGHT
    * As motion data, motion mask is rotated to 90 degree.
    */
    void setMotionMask(const QByteArray& mask);
    //void analizeFrame(const CLVideoDecoderOutput* frame);
    void analizeFrame(QnCompressedVideoDataPtr frame);
    QnMetaDataV1Ptr getMotion();
private:
    void scaleMask();
    void reallocateMask(int width, int height);
private:
    CLFFmpegVideoDecoder* m_decoder;
    CLVideoDecoderOutput m_outFrame;
    
    quint8* m_motionMask;
    quint8* m_scaledMask; // mask scaled to x * MD_HEIGHT. for internal use
    int m_scaledWidth;
    int m_xStep; // 8, 16, 24 e.t.c value
    int m_lastImgWidth;
    int m_lastImgHeight;
    quint8* m_frameBuffer[2];
    quint32* m_resultMotion;
    qint64 m_firstFrameTime;
    int m_totalFrames;
};

#endif // __MOTION_ESTIMATION_H__
