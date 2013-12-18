#ifndef __FISHEYE_CALIBRATOR_H_
#define __FISHEYE_CALIBRATOR_H_

#include "utils/media/frame_info.h"

/**
 * This class determine fisheye elipse center and radius
 * 
 */
class QnFisheyeCalibrator
{
public:
    QnFisheyeCalibrator();
    ~QnFisheyeCalibrator();

    /*
    *   analize frame. Can be called several times for different video frames to improve quality
    */
    void analizeFrame(QSharedPointer<CLVideoDecoderOutput> frame);
private:
    quint32 drawCircle(QSharedPointer<CLVideoDecoderOutput> frame, const QPoint& center, int radius);
    void findCircleParams();
    int findPixel(int y, int x, int xDelta);
    int findYThreshold(QSharedPointer<CLVideoDecoderOutput> frame);
private:
    QPointF m_center;
    qreal m_radius;
    quint8* m_filteredFrame;
    int m_width;
    int m_height;
};

#endif // __FISHEYE_CALIBRATOR_H_
