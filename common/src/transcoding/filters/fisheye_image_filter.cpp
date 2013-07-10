#include "fisheye_image_filter.h"

extern "C" {
#ifdef WIN32
#define AVPixFmtDescriptor __declspec(dllimport) AVPixFmtDescriptor
#endif
#include <libavutil/pixdesc.h>
#ifdef WIN32
#undef AVPixFmtDescriptor
#endif
};


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

    // complete weight vector
    return _mm_mul_ps(w_x, w_y);
}

inline quint8 GetPixelSSE3(quint8* buffer, int stride, float x, float y)
{
    __m128 weight = CalcWeights(x, y);

    const quint8* curPtr = buffer + (int)x + (int)y * stride; // pointer to first pixel
    __m128 data = _mm_setr_ps((float) curPtr[0], (float) curPtr[1], (float) curPtr[stride], (float) curPtr[stride+1]);

    union data {
        float fData[4];
        __m128 sseData;
    } memData;

    memData.sseData = _mm_mul_ps(data, weight);
    return quint8 (memData.fData[0] + memData.fData[1] + memData.fData[2] + memData.fData[3]);
    // return
    //return _mm_cvtsi128_si32(out);
}

// ------------ QnFisheyeImageFilter ----------------------

QnFisheyeImageFilter::QnFisheyeImageFilter(const DevorpingParams& params):
    QnAbstractImageFilter(),
    m_lastImageFormat(-1)
{
    memset (m_transform, 0, sizeof(m_transform));
}

void QnFisheyeImageFilter::updateImage(CLVideoDecoderOutput* frame, const QRectF& updateRect)
{
    int left = qPower2Floor(updateRect.left() * frame->width, 16);
    int right = qPower2Floor(updateRect.right() * frame->width, 16);
    int top = updateRect.top() * frame->height;
    int bottom = updateRect.bottom() * frame->height;
    QSize imageSize(right - left, bottom - top);

    const AVPixFmtDescriptor* descr = &av_pix_fmt_descriptors[frame->format];

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
            updateFisheyeTransform(QSize(w, h), plane);
        }
        m_lastImageSize = imageSize;
        m_lastImageFormat = frame->format;
    }

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
            quint8* srcLine = frame->data[plane] + y*frame->linesize[plane];
            for (int x = 0; x < w; ++x)
            {
                const QPointF* dstPixel = m_transform[plane] + index;
                quint8 pixel = GetPixelSSE3(frame->data[plane], frame->linesize[plane], dstPixel->x(), dstPixel->y());
                srcLine[x] = pixel;
                
                index++;
            }
        }
    }
}

void QnFisheyeImageFilter::updateFisheyeTransform(const QSize& imageSize, int plane)
{
    delete m_transform[plane];
    m_transform[plane] = new QPointF[imageSize.width() * imageSize.height()];

    qreal aspectRatio = imageSize.width() / (qreal) imageSize.height();
    qreal kx = 2.0*tan(m_params.fov/2.0);
    qreal ky = kx/aspectRatio;

    QMatrix4x4 rotX(kx,      0.0,                                0.0,                   0.0,
                    0.0,     cos(m_params.yAngle)*ky,            -sin(m_params.yAngle), 0.0,
                    0.0,     sin(m_params.yAngle)*ky,            cos(m_params.yAngle),  0.0,
                    0.0,     0.0,                                0.0,                   0.0);

    QPointF* dstPos = m_transform[plane];
    for (int y = 0; y < imageSize.height(); ++y)
    {
        for (int x = 0; x < imageSize.width(); ++x)
        {
            QVector3D pos3d(x / (qreal) (imageSize.width()-1) - 0.5, y / (qreal) (imageSize.height()-1) - 0.5, 1.0);
            pos3d = rotX * pos3d;  // 3d vector on surface, rotate and scale

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
