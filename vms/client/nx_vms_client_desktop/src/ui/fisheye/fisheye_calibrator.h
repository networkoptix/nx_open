#ifndef __FISHEYE_CALIBRATOR_H_
#define __FISHEYE_CALIBRATOR_H_

#include "utils/media/frame_info.h"
#include "nx/utils/thread/long_runnable.h"

/**
 * This class determine fisheye elipse center and radius
 *
 */
class QnFisheyeCalibrator: public QnLongRunnable
{
    Q_OBJECT
public:

    // Error codes for calibration process.
    enum Error
    {
        NoError,
        ErrorNotFisheyeImage,
        ErrorTooLowLight,
        ErrorInterrupted,   //< Process was interrupted before getting any result.
        ErrorInvalidInput,  //< Input image is invalid.
        ErrorInternal,      //< Some sort if internal error from ffmpeg
    };

    QnFisheyeCalibrator();
    ~QnFisheyeCalibrator();

    /*
    *   analyze frame. Can be called several times for different video frames to improve quality
    */
    void analyseFrameAsync(QImage frame);
    Error analyseFrame(QImage frame);

    void setCenter(const QPointF &center);
    QPointF center() const;

    void setHorizontalStretch(const qreal& value);
    qreal horizontalStretch() const;

    void setRadius(qreal radius);
    qreal radius() const;
signals:
    void centerChanged(const QPointF &center);
    void radiusChanged(qreal radius);
    void stretchChanged(qreal stretch);
    void finished(int errorCode);
protected:
    virtual void run();
private:
    //void drawCircle(const QImage& frame, const QPoint& center, int radius);
    Error findCircleParams();
    qreal findElipse(qreal& newRadius);
    int findPixel(int y, int x, int xDelta);
    int findYThreshold(QImage frame);
private:
    QPointF m_center;
    qreal m_horizontalStretch;
    qreal m_radius;
    quint8* m_filteredFrame;
    quint8* m_grayImageBuffer;
    int m_width;
    int m_height;

    QImage m_analysedFrame;
};

#endif // __FISHEYE_CALIBRATOR_H_
