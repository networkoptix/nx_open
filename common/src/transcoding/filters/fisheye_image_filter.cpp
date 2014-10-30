#include "fisheye_image_filter.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <QtGui/QMatrix4x4>

#include <utils/math/math.h>
#include <utils/media/frame_info.h>

extern "C" {
#ifdef WIN32
#   define AVPixFmtDescriptor __declspec(dllimport) AVPixFmtDescriptor
#endif
#include <libavutil/pixdesc.h>
#ifdef WIN32
#   undef AVPixFmtDescriptor
#endif
};

#ifdef Q_CC_GNU
/* There is a very strange bug in libm that results in sin/cos returning +-inf
 * for some values. The actual functions called are __sin_avx and __cos_avx.
 * This bug is worked around by invoking them for angles in [0, pi/2] range. */

// TODO: #Elric #2.3 #gcc Investigate further! Maybe it's fixed in new gcc release?

static qreal qSlowSin(qreal angle) {
    angle = qMod(angle, 2 * M_PI);
    if(angle < M_PI) {
        if(angle < 0.5 * M_PI) {
            return std::sin(angle);
        } else {
            return std::sin(M_PI - angle);
        }
    } else {
        if(angle < 1.5 * M_PI) {
            return -std::sin(angle - M_PI);
        } else {
            return -std::sin(2 * M_PI - angle);
        }
    }
}

static qreal qSlowCos(qreal angle) {
    angle = qMod(angle, 2 * M_PI);
    if(angle < M_PI) {
        if(angle < 0.5 * M_PI) {
            return std::cos(angle);
        } else {
            return -std::cos(M_PI - angle);
        }
    } else {
        if(angle < 1.5 * M_PI) {
            return -std::cos(angle - M_PI);
        } else {
            return std::cos(2 * M_PI - angle);
        }
    }
}

#define sin qSlowSin
#define cos qSlowCos
#endif // Q_CC_GNU

static bool saveTransformImage(const QPointF *transform, int width, int height, const QString &fileName) {
    QImage image(width, height, QImage::Format_ARGB32);

    int stride = image.bytesPerLine();
    unsigned char *line = (unsigned char *) image.scanLine(0);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            line[stride * y + x * 4 + 0] = transform[y * width + x].x() / width * 255;
            line[stride * y + x * 4 + 1] = transform[y * width + x].y() / height * 255;
            line[stride * y + x * 4 + 2] = 0;
            line[stride * y + x * 4 + 3] = 255;
        }
    }
    
    return image.save(fileName);
}


#if defined(__i386) || defined(__amd64) || defined(_WIN32)
// constant values that will be needed
static const __m128 CONST_1111 = _mm_set1_ps(1);
static const __m128 CONST_256 = _mm_set1_ps(256);

inline __m128 CalcWeights(float x, float y)
{
    __m128 ssx = _mm_set_ss(x);
    __m128 ssy = _mm_set_ss(y);
    __m128 psXY = _mm_unpacklo_ps(ssx, ssy);      // 0 0 y x

    //__m128 psXYfloor = _mm_floor_ps(psXY); // use this line for if you have SSE4
    __m128 psXYfloor = _mm_cvtepi32_ps(_mm_cvtps_epi32(psXY));
    __m128 psXYfrac = _mm_sub_ps(psXY, psXYfloor); // = frac(psXY)

    __m128 psXYfrac1 = _mm_sub_ps(CONST_1111, psXYfrac); // ? ? (1-y) (1-x)
    __m128 w_x = _mm_unpacklo_ps(psXYfrac1, psXYfrac);   // ? ?     x (1-x)
    w_x = _mm_movelh_ps(w_x, w_x);      // x (1-x) x (1-x)
    __m128 w_y = _mm_shuffle_ps(psXYfrac1, psXYfrac, _MM_SHUFFLE(1, 1, 1, 1)); // y y (1-y) (1-y)

    // complete weight vectorqFastCos
    return _mm_mul_ps(w_x, w_y);
}

static inline quint8 GetPixel(quint8* buffer, int stride, float x, float y)
{
    simd128 weight = CalcWeights(x, y);

    const quint8* curPtr = buffer + (int)x + (int)y * stride; // pointer to first pixel
    simd128 data = _mm_setr_ps((float) curPtr[0], (float) curPtr[1], (float) curPtr[stride], (float) curPtr[stride+1]);

    union data {
        float fData[4];
        simd128 sseData;
    } memData;

    memData.sseData = _mm_mul_ps(data, weight);
    return quint8 (memData.fData[0] + memData.fData[1] + memData.fData[2] + memData.fData[3]);
    // return
    //return _mm_cvtsi128_si32(out);
}
#elif __arm__ && __ARM_NEON__
static inline quint8 GetPixel(quint8* buffer, int stride, float x, float y)
{
    //TODO/ARM
    return 0;
}
#else
static inline quint8 GetPixel(quint8* buffer, int stride, float x, float y)
{
    //TODO: C fallback routine
    return 0;
}
#endif

// ------------ QnFisheyeImageFilter ----------------------

QnFisheyeImageFilter::QnFisheyeImageFilter(const QnMediaDewarpingParams& mediaDewarping, const QnItemDewarpingParams& itemDewarping):
    QnAbstractImageFilter(),
    m_mediaDewarping(mediaDewarping),
    m_itemDewarping(itemDewarping),
    m_tmpBuffer(new CLVideoDecoderOutput()),
    m_lastImageFormat(-1)
{
    memset (m_transform, 0, sizeof(m_transform));
}

CLVideoDecoderOutputPtr QnFisheyeImageFilter::updateImage(const CLVideoDecoderOutputPtr& frame, const QRectF& updateRect, qreal ar)
{
    if (!m_itemDewarping.enabled || !m_mediaDewarping.enabled)
        return frame;

    int left = qPower2Floor(updateRect.left() * frame->width, 16);
    int right = qPower2Floor(updateRect.right() * frame->width, 16);
    int top = updateRect.top() * frame->height;
    int bottom = updateRect.bottom() * frame->height;
    QSize imageSize(right - left, bottom - top);

    const AVPixFmtDescriptor* descr = &av_pix_fmt_descriptors[frame->format];

#if defined(__i386) || defined(__amd64) || defined(_WIN32)
    int prevRoundMode = _MM_GET_ROUNDING_MODE();
    _MM_SET_ROUNDING_MODE(_MM_ROUND_TOWARD_ZERO);
#elif __arm__ && __ARM_NEON__
    //TODO/ARM
#else
    //TODO: C fallback routine
#endif

    if (imageSize != m_lastImageSize || frame->format != m_lastImageFormat) 
    {
        for (int plane = 0; plane < descr->nb_components && frame->data[plane]; ++plane)
        {
            int w = imageSize.width();
            int h = imageSize.height();
            if (plane > 0) {
                w >>= descr->log2_chroma_w;
                h >>= descr->log2_chroma_h;
            }
            updateFisheyeTransform(QSize(w, h), plane, ar);
        }
        m_lastImageSize = imageSize;
        m_lastImageFormat = frame->format;
        m_tmpBuffer->reallocate(frame->width, frame->height, frame->format);
    }

    m_tmpBuffer->copyDataFrom(frame.data());

    for (int plane = 0; plane < descr->nb_components && frame->data[plane]; ++plane)
    {
        int index = 0;
        int w = imageSize.width();
        int h = imageSize.height();
        if (plane > 0) {
            w >>= descr->log2_chroma_w;
            h >>= descr->log2_chroma_h;
        }

        for (int y = 0; y < h; ++y)
        {
            quint8* dstLine = frame->data[plane] + y*frame->linesize[plane];
            for (int x = 0; x < w; ++x)
            {
                const QPointF* dstPixel = m_transform[plane] + index;
                quint8 pixel = GetPixel(m_tmpBuffer->data[plane], m_tmpBuffer->linesize[plane], dstPixel->x(), dstPixel->y());
                dstLine[x] = pixel;
                
                index++;
            }
        }
    }
#if defined(__i386) || defined(__amd64) || defined(_WIN32)
    _MM_SET_ROUNDING_MODE(prevRoundMode);
#elif __arm__ && __ARM_NEON__
    //TODO/ARM
#else
    //TODO: C fallback routine
#endif
    return frame;
}

QVector3D sphericalToCartesian(qreal theta, qreal phi) { // TODO: #Elric use function from coordinate_transform header
    return QVector3D(cos(phi) * sin(theta), cos(phi)*cos(theta), sin(phi));
}

void QnFisheyeImageFilter::updateFisheyeTransform(const QSize& imageSize, int plane, qreal ar)
{
    delete m_transform[plane];
    m_transform[plane] = new QPointF[imageSize.width() * imageSize.height()];

    if (m_itemDewarping.panoFactor == 1)
        updateFisheyeTransformRectilinear(imageSize, plane, ar);
    else
        updateFisheyeTransformEquirectangular(imageSize, plane, ar);

    //saveTransformImage(m_transform[plane], imageSize.width(), imageSize.height(), lit("/home/alexandra/transform_%1.png").arg(plane));
}

void QnFisheyeImageFilter::updateFisheyeTransformRectilinear(const QSize& imageSize, int plane, qreal aspectRatio)
{
    qreal kx = 2.0*tan(m_itemDewarping.fov/2.0);
    qreal ky = kx/aspectRatio;

    float fovRot = sin(m_itemDewarping.xAngle)*qDegreesToRadians(m_mediaDewarping.fovRot);
    qreal xShift, yShift, yPos;
    if (m_mediaDewarping.viewMode == QnMediaDewarpingParams::Horizontal) {
        yShift = m_itemDewarping.yAngle;
        yPos = 0.5;
        xShift = m_itemDewarping.xAngle;
        fovRot = fovRot;
    }
    else {
        yShift = m_itemDewarping.yAngle - M_PI/2.0 - m_itemDewarping.fov/2.0/aspectRatio;
        yPos =  1.0;
        xShift = fovRot;
        fovRot =  -m_itemDewarping.xAngle;
    }
    qreal radius = m_mediaDewarping.radius;
    qreal xCenter = m_mediaDewarping.xCenter;
    qreal yCenter = m_mediaDewarping.yCenter;

    aspectRatio /= m_mediaDewarping.hStretch;

    QVector3D dx = sphericalToCartesian(xShift + M_PI/2.0, 0.0) * kx;
    QVector3D dy = sphericalToCartesian(xShift, -yShift + M_PI/2.0) * kx / m_mediaDewarping.hStretch;
    QVector3D center = sphericalToCartesian(xShift, -yShift);

    QMatrix4x4 to3d(dx.x(),     dy.x(),     center.x(),     0.0,
                    dx.y(),     dy.y(),     center.y(),     0.0,
                    dx.z(),     dy.z(),     center.z(),     0.0,
                    0.0,        0.0,        0.0,            1.0);

    QPointF* dstPos = m_transform[plane];
    int dstDelta = 1;
    if (m_mediaDewarping.viewMode == QnMediaDewarpingParams::VerticalDown) {
        dstPos += imageSize.height()*imageSize.width() - 1;
        dstDelta = -1;
    }

    QVector2D xy1 = QVector2D(1.0,         1.0 / aspectRatio);
    QVector2D xy2 = QVector2D(-0.5,        -yPos/ aspectRatio);

    //QVector2D xy3 = QVector2D(1.0 / M_PI,  aspectRatio / M_PI);
    //QVector2D xy4 = QVector2D(1.0 / 2.0,   1.0 / 2.0);

    QVector2D xy3 = QVector2D(1.0 / M_PI * radius*2.0,  1.0 / M_PI * radius*2.0*aspectRatio);
    QVector2D xy4 = QVector2D(xCenter, yCenter);


    for (int y = 0; y < imageSize.height(); ++y)
    {
        for (int x = 0; x < imageSize.width(); ++x)
        {
            QVector2D pos = QVector2D(x / qreal(imageSize.width()-1), y / qreal(imageSize.height()-1)) * xy1 + xy2;
            QVector3D pos3d(pos.x(), pos.y(), 1.0);
            pos3d = to3d * pos3d;  // 3d vector on surface, rotate and scale

            qreal theta = atan2(pos3d.z(), pos3d.x()) + fovRot;     // fisheye angle
            qreal r     = acos(pos3d.y() / pos3d.length());
            if (qIsNaN(r))
                r = 0.0;

            pos = QVector2D(cos(theta), sin(theta)) * r;
            pos = pos * xy3 + xy4;

            qreal dstX = pos.x() * (imageSize.width()-1);
            qreal dstY = pos.y() * (imageSize.height()-1);

            if (dstX < 0.0 || dstX > (qreal) (imageSize.width() - 1) ||
                dstY < 0.0 || dstY > (qreal) (imageSize.height() - 1))
            {
                dstX = 0.0;
                dstY = 0.0;
            }

            *dstPos = QPointF(dstX, dstY);
            dstPos += dstDelta;
        }
    }
}

void QnFisheyeImageFilter::updateFisheyeTransformEquirectangular(const QSize& imageSize, int plane, qreal aspectRatio)
{
    qreal fovRot = qDegreesToRadians(m_mediaDewarping.fovRot);
    QMatrix4x4 perspectiveMatrix( 
        1.0,    0.0,               0.0,               0.0,
        0.0,    cos(-fovRot),     -sin(-fovRot),      0.0,
        0.0,    sin(-fovRot),      cos(-fovRot),      0.0,
        0.0,    0.0,               0.0,               1.0
    );

    qreal yPos;
    qreal phiShiftSign;
    qreal yAngle = m_itemDewarping.yAngle;
    if (m_mediaDewarping.viewMode == QnMediaDewarpingParams::Horizontal) {
        yPos = 0.5;
        phiShiftSign = 1.0;
    }
    else {
        yPos =  1.0;
        phiShiftSign = -1.0;
        yAngle -= m_itemDewarping.fov/m_itemDewarping.panoFactor/2.0;
    }
    qreal radius = m_mediaDewarping.radius;
    qreal xCenter = m_mediaDewarping.xCenter;
    qreal yCenter = m_mediaDewarping.yCenter;

    QPointF* dstPos = m_transform[plane];
    
    int dstDelta = 1;
    if (m_mediaDewarping.viewMode == QnMediaDewarpingParams::VerticalDown) {
        dstPos += imageSize.height()*imageSize.width() - 1;
        dstDelta = -1;
    }

    aspectRatio /= m_mediaDewarping.hStretch;

    QVector2D xy1 = QVector2D(m_itemDewarping.fov, (m_itemDewarping.fov / m_itemDewarping.panoFactor));
    QVector2D xy2 = QVector2D(-0.5,  -yPos)*xy1 + QVector2D(m_itemDewarping.xAngle, 0.0);

    QVector2D xy3 = QVector2D(1.0 / M_PI * radius*2.0,  1.0 / M_PI * radius*2.0*aspectRatio);
    QVector2D xy4 = QVector2D(xCenter, yCenter);

    for (int y = 0; y < imageSize.height(); ++y)
    {
        for (int x = 0; x < imageSize.width(); ++x)
        {
            QVector2D pos = QVector2D(x / (qreal) (imageSize.width()-1), y / (qreal) (imageSize.height()-1)) * xy1 + xy2;

            float cosTheta = cos(pos.x());
            float roty = -fovRot * cosTheta;
            float phi   = phiShiftSign * (pos.y() * (1.0 - roty*xy1.y())  - roty - yAngle);
            float cosPhi = cos(phi);

            // Vector in 3D space
            QVector3D pos3d;
            if (m_mediaDewarping.viewMode == QnMediaDewarpingParams::Horizontal)
                pos3d = QVector3D(cosPhi * sin(pos.x()),    cosPhi * cosTheta, sin(phi));
            else
                pos3d = QVector3D(cosPhi * sin(pos.x()),    sin(phi),          cosPhi * cosTheta);
            QVector3D psph = perspectiveMatrix * pos3d;

            // Calculate fisheye angle and radius
            float theta = atan2(psph.z(), psph.x());
            float r = acos(psph.y());
            if (qIsNaN(r))
                r = 0.0;

            // return from polar coordinates
            pos = QVector2D(cos(theta), sin(theta)) * r;
            pos = pos * xy3 + xy4;
            
            qreal dstX = pos.x() * (imageSize.width()-1);
            qreal dstY = pos.y() * (imageSize.height()-1);

            if (dstX < 0.0 || dstX > (qreal) (imageSize.width() - 1) ||
                dstY < 0.0 || dstY > (qreal) (imageSize.height() - 1))
            {
                dstX = 0.0;
                dstY = 0.0;
            }
            
            *dstPos = QPointF(dstX, dstY);
            dstPos += dstDelta;
        }
    }
}

QSize QnFisheyeImageFilter::getOptimalSize(const QSize& srcResolution, const QnItemDewarpingParams &itemDewarpingParams)
{
    if (!itemDewarpingParams.enabled || itemDewarpingParams.panoFactor == 1)
        return srcResolution;

    int square = srcResolution.width() * srcResolution.height();
    float aspect = itemDewarpingParams.panoFactor;
    int x = int(sqrt(square * aspect) + 0.5);
    int y = int(x /aspect + 0.5);
    return QSize(qPower2Floor(x, 16), qPower2Floor(y, 2));
}

#endif // ENABLE_DATA_PROVIDERS
