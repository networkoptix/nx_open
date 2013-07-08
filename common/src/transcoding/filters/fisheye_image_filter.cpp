#include "fisheye_image_filter.h"

QnFisheyeImageFilter::QnFisheyeImageFilter(const DevorpingParams& params):
    QnAbstractImageFilter(),
    m_transform(0)
{

}

void QnFisheyeImageFilter::updateImage(CLVideoDecoderOutput* frame, const QRectF& updateRect)
{
    int left = qPower2Floor(updateRect.left() * frame->width, 16);
    int right = qPower2Floor(updateRect.right() * frame->width, 16);
    int top = updateRect.top() * frame->height;
    int bottom = updateRect.bottom() * frame->height;
    QSize imageSize(right - left, bottom - top);
    if (imageSize != m_lastImageSize) {
        updateFisheyeTransform(imageSize);
        m_lastImageSize = imageSize;
    }
}

void QnFisheyeImageFilter::updateFisheyeTransform(const QSize& imageSize)
{
    delete m_transform;
    m_transform = new QPointF[imageSize.width() * imageSize.height()];

    qreal aspectRatio = imageSize.width() / (qreal) imageSize.height();
    qreal kx = 2.0*tan(m_params.fov/2.0);
    qreal ky = kx/aspectRatio;

    QMatrix4x4 rotX(kx,      0.0,                                0.0,                   0.0,
                    0.0,     cos(m_params.yAngle)*ky,            -sin(m_params.yAngle), 0.0,
                    0.0,     sin(m_params.yAngle)*ky,            cos(m_params.yAngle),  0.0,
                    0.0,     0.0,                                0.0,                   0.0);

    QPointF* dstPos = m_transform;
    for (int y = 0; y < imageSize.height(); ++y)
    {
        for (int x = 0; x < imageSize.width(); ++x)
        {
            QVector3D pos3d(x / (qreal) (imageSize.width()-1) - 0.5, y / (qreal) (imageSize.height()-1) - 0.5, 1.0);
            pos3d = rotX * pos3d;

            qreal theta = atan2(pos3d.x(), pos3d.z()) + m_params.xAngle;
            qreal z = pos3d.y() / pos3d.length();

            // Vector on 3D sphere
            qreal k   = cos(asin(z)); // same as sqrt(1-z*z)
            QVector3D psph(k * sin(theta), k * cos(theta), z);

            // Calculate fisheye angle and radius
            theta      = atan2(psph.z(), psph.x());
            qreal r  = atan2(QVector2D(psph.x(), psph.z()).length(), psph.y()) / M_PI;

            // return from polar coordinates
            qreal dstX = qBound(0.0, cos(theta) * r + 0.5, (qreal) imageSize.width());
            qreal dstY = qBound(0.0, sin(theta) * r + 0.5, (qreal) imageSize.height());

            *dstPos++ = QPointF(dstX, dstY);
        }
    }
}
