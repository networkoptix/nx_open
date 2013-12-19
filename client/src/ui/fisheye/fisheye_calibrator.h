#ifndef __FISHEYE_CALIBRATOR_H_
#define __FISHEYE_CALIBRATOR_H_

#include "utils/media/frame_info.h"
#include "utils/common/long_runnable.h"

/**
 * This class determine fisheye elipse center and radius
 * 
 */
class QnFisheyeCalibrator: public QnLongRunnable
{
    Q_OBJECT
public:
    QnFisheyeCalibrator();
    ~QnFisheyeCalibrator();

    /*
    *   analize frame. Can be called several times for different video frames to improve quality
    */
    void analizeFrame(QImage frame);
    void analyseFrameAsync(QImage frame);

    void setCenter(const QPointF &center);
    QPointF center() const;

    void setRadius(qreal radius);
    qreal radius() const;
signals:
    void centerChanged(const QPointF &center);
    void radiusChanged(qreal radius);

    void finished();
protected:
    virtual void run();
private:
    quint32 drawCircle(QSharedPointer<CLVideoDecoderOutput> frame, const QPoint& center, int radius);
    void findCircleParams();
    int findPixel(int y, int x, int xDelta);
    int findYThreshold(QSharedPointer<CLVideoDecoderOutput> frame);
private:
    QPointF m_center;
    qreal m_radius;
    quint8* m_filteredFrame;
    quint8* m_grayImageBuffer;
    int m_width;
    int m_height;

    QImage m_analysedFrame;
};

#endif // __FISHEYE_CALIBRATOR_H_
