#ifndef __MOTION_ESTIMATION_H__
#define __MOTION_ESTIMATION_H__

#include <QByteArray>
#include "core/datapacket/mediadatapacket.h"
#include "decoders/video/ffmpeg.h"
#include "core/resource/motion_window.h"

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
    bool existsMetadata() const;

    static QByteArray createSoftwareMotionMask(const QnMotionRegion& region);
private:
    void scaleMask();
    void reallocateMask(int width, int height);
    void postFiltering();
    void analizeMotionAmount(quint8* frame);
private:
    QMutex m_mutex;
    CLFFmpegVideoDecoder* m_decoder;
    CLVideoDecoderOutput* m_frames[2];
    
    quint8* m_motionMask;
    quint8* m_scaledMask; // mask scaled to x * MD_HEIGHT. for internal use
    int m_scaledWidth;
    int m_xStep; // 8, 16, 24 e.t.c value
    int m_lastImgWidth;
    int m_lastImgHeight;
    quint8* m_frameBuffer[2];
    quint32* m_resultMotion;
    qint64 m_firstFrameTime;
    qint64 m_lastFrameTime;
    int m_totalFrames;
    bool m_isNewMask;

    int m_linkedMap[MD_WIDTH*MD_HEIGHT];
    int m_linkedNums[MD_WIDTH*MD_HEIGHT];
    int m_linkedSquare[MD_WIDTH*MD_HEIGHT];
};

#endif // __MOTION_ESTIMATION_H__
