// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "dewarping_image_filter.h"

#include <QtCore/QtMath>

#include <nx/media/config.h>
#include <nx/media/ffmpeg/frame_info.h>
#include <nx/media/sse_helper.h>
#include <nx/utils/math/math.h>

using namespace nx::vms::api;

namespace {

static constexpr auto k360VRAspectRatio = 16.0 / 9.0;

QVector3D operator*(const QMatrix3x3& m, const QVector3D& v)
{
    return QVector3D(m(0, 0) * v.x() + m(0, 1) * v.y() + m(0, 2) * v.z(),
        m(1, 0) * v.x() + m(1, 1) * v.y() + m(1, 2) * v.z(),
        m(2, 0) * v.x() + m(2, 1) * v.y() + m(2, 2) * v.z());
}

QVector2D operator*(const QMatrix3x3& m, const QVector2D& v)
{
    return QVector2D(m * QVector3D(v, 1.0));
}

} // namespace

extern "C" {
#ifdef WIN32
    #define AVPixFmtDescriptor __declspec(dllimport) AVPixFmtDescriptor
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

// TODO: #dklychkov Investigate further. Maybe it's fixed in new gcc release?

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

#if 0
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
#endif

#if defined(NX_SSE2_SUPPORTED)

inline __m128 calcWeights(float x, float y)
{
    __m128 value = _mm_set_ps(y, x, y, x);
    __m128 floorValue = _mm_cvtepi32_ps(_mm_cvttps_epi32(value));
    __m128 fracValue1 = _mm_sub_ps(value, floorValue);
    __m128 fracValue2 = _mm_sub_ps(_mm_set1_ps(1), fracValue1); //< 1-.y, 1-.x, 1-.y, 1-.x
    __m128 fracValue = _mm_unpacklo_ps(fracValue2, fracValue1); //< .y, 1-.y, .x, 1-.x

    __m128 xWeight = _mm_shuffle_ps(fracValue, fracValue, _MM_SHUFFLE(1, 0, 1, 0)); //<  .x (1-.x) .x (1-.x)
    __m128 yWeight = _mm_shuffle_ps(fracValue, fracValue, _MM_SHUFFLE(3, 3, 2, 2)); //<  .y, .y, (1-.y), (1-.y),
    return _mm_mul_ps(xWeight, yWeight);
}

quint8 QnDewarpingImageFilter::getPixel(quint8* buffer, int stride, float x, float y, int width, int height)
{
    simd128 weight = calcWeights(x, y);

    const quint8* curPtr = buffer + (int)x + (int)y * stride; // pointer to first pixel
    simd128 data;
    if (x < width - 1 && y < height - 1)
        data = _mm_setr_ps((float)curPtr[0], (float)curPtr[1], (float)curPtr[stride], (float)curPtr[stride + 1]);
    else if (x < width - 1)
        data = _mm_setr_ps((float)curPtr[0], (float)curPtr[1], (float)curPtr[0], (float)curPtr[1]);
    else if (y < height - 1)
        data = _mm_setr_ps((float)curPtr[0], (float)curPtr[0], (float)curPtr[stride], (float)curPtr[stride]);
    else
        data = _mm_setr_ps((float)curPtr[0], (float)curPtr[0], (float)curPtr[0], (float)curPtr[0]);

    union data {
        float fData[4];
        simd128 sseData;
    } memData;

    memData.sseData = _mm_mul_ps(data, weight);
    return quint8 (memData.fData[0] + memData.fData[1] + memData.fData[2] + memData.fData[3]);
    // return
    //return _mm_cvtsi128_si32(out);
}
#else
quint8 QnDewarpingImageFilter::getPixel(quint8* buffer, int stride, float x, float y, int width, int height)
{
    // TODO: C fallback routine
    return 0;
}
#endif

// ------------------------------------------------------------------------------------------------
// QnDewarpingImageFilter

QnDewarpingImageFilter::QnDewarpingImageFilter():
    QnAbstractImageFilter()
{
}

QnDewarpingImageFilter::QnDewarpingImageFilter(
    const dewarping::MediaData& mediaParams,
    const dewarping::ViewData& itemParams)
    :
    QnDewarpingImageFilter()
{
    setParameters(mediaParams, itemParams);
}

void QnDewarpingImageFilter::setParameters(
    const dewarping::MediaData& mediaParams,
    const dewarping::ViewData& itemParams)
{
    m_mediaParams = mediaParams;
    m_itemParams = itemParams;
    m_lastImageFormat = -1;
}

CLVideoDecoderOutputPtr QnDewarpingImageFilter::updateImage(
    const CLVideoDecoderOutputPtr& srcFrame,
    const QnAbstractCompressedMetadataPtr&)
{
    if (!m_itemParams.enabled || !m_mediaParams.enabled)
        return srcFrame;

    CLVideoDecoderOutputPtr frame;
    const auto updatedSize = updatedResolution(srcFrame->size());
    if (updatedSize != srcFrame->size())
        frame = srcFrame->scaled(updatedSize);
    else
        frame = srcFrame;

    // `scaled()` can return an empty pointer. Better process as is than crash.
    if (!NX_ASSERT(frame, "Error while scaling frame to %1", updatedSize))
        frame = srcFrame;

    int left = 0;
    int right = frame->width;
    int top = 0;
    int bottom = frame->height;
    QSize imageSize(right - left, bottom - top);

    const AVPixFmtDescriptor* descr = av_pix_fmt_desc_get((AVPixelFormat) frame->format);

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
            qreal sar = srcFrame->sample_aspect_ratio ? srcFrame->sample_aspect_ratio : 1.0;
            qreal ar = srcFrame->width * sar / (qreal) srcFrame->height;
            updateTransformMap(QSize(w, h), plane, ar);
        }
        m_lastImageSize = imageSize;
        m_lastImageFormat = frame->format;
    }

    CLVideoDecoderOutputPtr outputFrame(new CLVideoDecoderOutput);
    outputFrame->copyFrom(frame.get());

    for (int plane = 0; plane < descr->nb_components && outputFrame->data[plane]; ++plane)
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
            quint8* dstLine = outputFrame->data[plane] + y * outputFrame->linesize[plane];
            for (int x = 0; x < w; ++x)
            {
                const QPointF* dstPixel = m_transform[plane].data() + index;
                quint8 pixel = getPixel(frame->data[plane],
                    frame->linesize[plane], dstPixel->x(), dstPixel->y(), w, h);
                dstLine[x] = pixel;
                index++;
            }
        }
    }
    return outputFrame;
}

void QnDewarpingImageFilter::updateDewarpingParameters(qreal aspectRatio)
{
    const bool isPanoramic = m_itemParams.panoFactor > 1;

    const bool isSpherical = dewarping::MediaData::is360VR(m_mediaParams.cameraProjection);
    const bool isHorizontal = isSpherical
        || m_mediaParams.viewMode == dewarping::FisheyeCameraMount::wall;

    const qreal yPos = isHorizontal ? 0.5 : 1.0;

    const auto scaledAspectRatio = aspectRatio / m_mediaParams.hStretch;

    const auto xAngle = float(isHorizontal ? m_itemParams.xAngle : 0.0);

    const auto maxX = 1.0;
    const auto maxY = 1.0;

    m_viewTransform.setToIdentity();
    m_cameraImageTransform.setToIdentity();
    m_unprojectionTransform.setToIdentity();
    m_sphereRotationTransform.setToIdentity();
    m_horizonCorrectionTransform.setToIdentity();

    if (isSpherical)
    {
        // 360 VR.

        const float centerX = float(maxX * 0.5);
        const float centerY = float(maxY * 0.5);

        const float cameraImageTransform[9]{
            float(centerX / M_PI), 0.0f, centerX,
            0.0f, float(centerY / M_PI * aspectRatio), centerY,
            0.0f, 0.0f, 1.0f};

        m_cameraImageTransform = QMatrix3x3(cameraImageTransform);

        m_horizonCorrectionTransform.rotate(-m_mediaParams.sphereAlpha, 0, 0, 1);
        m_horizonCorrectionTransform.rotate(-m_mediaParams.sphereBeta, 1, 0, 0);
        m_horizonCorrectionTransform.rotate(m_mediaParams.sphereAlpha, 0, 0, 1);
    }
    else
    {
        // Fisheye.

        const float cameraRoll = float(qDegreesToRadians(m_mediaParams.fovRot) -
            (isHorizontal ? 0.0 : (m_itemParams.xAngle - M_PI)));

        const float rollSin = sinf(cameraRoll);
        const float rollCos = cosf(cameraRoll);

        const float cameraRollMatrix[9]{
            rollCos, -rollSin, 0.0f,
            rollSin, rollCos, 0.0f,
            0.0f, 0.0f, 1.0f};

        const float cameraImageTransform[9]{
            float(maxX * m_mediaParams.radius), 0.0f, float(maxX * m_mediaParams.xCenter),
            0.0f, float(maxY * m_mediaParams.radius * scaledAspectRatio),
            float(maxY * m_mediaParams.yCenter),
            0.0f, 0.0f, 1.0f};

        m_cameraImageTransform = QMatrix3x3(cameraImageTransform) * QMatrix3x3(cameraRollMatrix);
    }

    if (isPanoramic)
    {
        // Equirectangular view projection.

        const auto yAngle = isHorizontal
            ? m_itemParams.yAngle
            : m_itemParams.yAngle - m_itemParams.fov / m_itemParams.panoFactor / 2.0;

        const float viewTransform[9]{
            float(m_itemParams.fov / maxX), 0.0f, float(-0.5 * m_itemParams.fov + xAngle),
            0.0f, float((m_itemParams.fov / m_itemParams.panoFactor) / maxY),
            float(-yPos * m_itemParams.fov / m_itemParams.panoFactor - yAngle),
            0.0f, 0.0f, 1.0f};

        static const float kVerticalSwizzleMatrix[9] = {
            1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, -1.0f,
            0.0f, 1.0f, 0.0f};

        m_viewTransform = QMatrix3x3(viewTransform);

        m_sphereRotationTransform = isHorizontal
            ? QMatrix3x3()
            : QMatrix3x3(kVerticalSwizzleMatrix);
    }
    else
    {
        // Rectilinear view projection.

        const auto yAngle = isHorizontal
            ? m_itemParams.yAngle
            : m_itemParams.yAngle - M_PI / 2.0 - m_itemParams.fov / (2.0 * aspectRatio);

        const float kx = float(2.0 * tan(m_itemParams.fov / 2.0));
        const float ky = float(kx / m_mediaParams.hStretch);

        const float sinX = sinf(float(xAngle));
        const float cosX = cosf(float(xAngle));
        const float sinY = sinf(float(yAngle));
        const float cosY = cosf(float(yAngle));

        const float unprojectionTransform[9]{
            cosX * kx, sinX * sinY * ky, sinX * cosY,
            -sinX * kx, cosX * sinY * ky, cosX * cosY,
            0.0f, cosY * ky, -sinY};

        const float viewTransform[9]{
            1.0f / float(maxX), 0.0f, -0.5f,
            0.0f, 1.0f / float(maxY * scaledAspectRatio), float(-yPos / scaledAspectRatio),
            0.0f, 0.0f, 1.0f};

        m_viewTransform = QMatrix3x3(viewTransform);
        m_unprojectionTransform = QMatrix3x3(unprojectionTransform);
    }

    if (!isSpherical && m_mediaParams.viewMode == dewarping::FisheyeCameraMount::ceiling)
    {
        static const float kViewportRotate180[9]{-1, 0, 1, 0, -1, 1, 0, 0, 1}; //< Around {0.5, 0.5}
        m_viewTransform = m_viewTransform * QMatrix3x3(kViewportRotate180);
    }
}

QPointF QnDewarpingImageFilter::transformed(int x, int y, const QSize& imageSize) const
{
    const QVector2D dimensions(imageSize.width(), imageSize.height());

    const QVector2D viewCoords = m_viewTransform * (QVector2D(x, y) / dimensions);

    const QVector3D sphereCoords = viewUnproject(viewCoords).normalized();

    const QVector2D imageCoords =
        m_cameraImageTransform * cameraProject(sphereCoords) * dimensions;

    return QPointF(
        qBound<float>(0, imageCoords.x(), imageSize.width() - 1),
        qBound<float>(0, imageCoords.y(), imageSize.height() - 1));
}

void QnDewarpingImageFilter::updateTransformMap(
    const QSize& imageSize, int plane, qreal aspectRatio)
{
    updateDewarpingParameters(aspectRatio);

    m_transform[plane] = PointsVector(imageSize.width() * imageSize.height());
    auto dstIt = m_transform[plane].begin();

    for (int y = 0; y < imageSize.height(); ++y)
    {
        for (int x = 0; x < imageSize.width(); ++x)
        {
            *dstIt = transformed(x, y, imageSize);
            ++dstIt;
        }
    }
}

QVector2D QnDewarpingImageFilter::cameraProject(const QVector3D& pointOnSphere) const
{
    const QVector2D xz = QVector2D(pointOnSphere.x(), pointOnSphere.z());
    const float y = pointOnSphere.y();

    switch (m_mediaParams.cameraProjection)
    {
        case dewarping::CameraProjection::equidistant:
        {
            const float theta = acos(std::clamp(y, -1.0f, 1.0f));
            return xz.normalized() * theta / (M_PI / 2);
        }

        case dewarping::CameraProjection::stereographic:
        {
            return xz / (1.0 + y);
        }

        case dewarping::CameraProjection::equisolid:
        {
            return xz / sqrt(1.0 + y);
        }

        case dewarping::CameraProjection::equirectangular360:
        {
            const auto point = m_horizonCorrectionTransform * pointOnSphere;
            return QVector2D(atan2(point.x(), point.y()), asin(std::clamp(point.z(), -1.0f, 1.0f)));
        }
    }

    NX_ASSERT(false, "Unknown camera projection");
    return QVector2D();
}

QVector3D QnDewarpingImageFilter::viewUnproject(const QVector2D& viewCoords) const
{
    const auto viewProjection = m_itemParams.panoFactor > 1.0
        ? dewarping::ViewProjection::equirectangular
        : dewarping::ViewProjection::rectilinear;

    switch (viewProjection)
    {
        case dewarping::ViewProjection::rectilinear:
            return m_unprojectionTransform * QVector3D(viewCoords.x(), viewCoords.y(), 1.0);

        case dewarping::ViewProjection::equirectangular:
        {
            const float cosTheta = cosf(viewCoords.x());
            const float sinTheta = sinf(viewCoords.x());

            const float cosPhi = cosf(viewCoords.y());
            const float sinPhi = sinf(viewCoords.y());

            return m_sphereRotationTransform
                * QVector3D(cosPhi * sinTheta, cosPhi * cosTheta, sinPhi);
        }
    }

    NX_ASSERT(false, "Unknown view projection");
    return QVector3D();
}

QSize QnDewarpingImageFilter::getOptimalSize(
    const QSize& srcResolution,
    const dewarping::MediaData& mediaParamsParams,
    const dewarping::ViewData& itemParamsParams)
{
    if (!itemParamsParams.enabled
        || (itemParamsParams.panoFactor == 1 && !mediaParamsParams.is360VR()))
    {
        return srcResolution;
    }

    const int square = srcResolution.width() * srcResolution.height();
    const float aspect = mediaParamsParams.is360VR() && itemParamsParams.panoFactor == 1
        ? k360VRAspectRatio
        : itemParamsParams.panoFactor;

    const int x = int(sqrt(square * aspect) + 0.5);
    const int y = int(x / aspect + 0.5);
    return QSize(qPower2Floor(x, 16), qPower2Floor(y, 2));
}

QSize QnDewarpingImageFilter::updatedResolution(const QSize& srcSize)
{
    return getOptimalSize(srcSize, m_mediaParams, m_itemParams);
}
