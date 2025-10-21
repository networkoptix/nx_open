// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "pixelation_image_filter.h"

#include <optional>
#include <QImage>

#include <analytics/common/object_metadata.h>
#include <nx/fusion/serialization/ubjson.h>
#include <nx/media/ffmpeg/frame_info.h>

extern "C" {
    #include <libavutil/pixdesc.h>
};

namespace nx::core::transcoding {
namespace {

constexpr int kIntensityToInt = 10;

std::optional<nx::common::metadata::ObjectMetadataPacket> objectDataPacketFromMetadata(
    const QnConstAbstractCompressedMetadataPtr& metadata)
{
    const auto compressedMetadata = std::dynamic_pointer_cast<const QnCompressedMetadata>(metadata);
    if (!compressedMetadata)
        return {};

    return QnUbjson::deserialized<nx::common::metadata::ObjectMetadataPacket>(
        QByteArray::fromRawData(compressedMetadata->data(), int(compressedMetadata->dataSize())));
}

void blur(uint8_t* data, int w, int h, int stride, int radius)
{
    std::vector<int64_t> integration((w + 1) * (h + 1), 0);
    std::vector<int64_t*> integrationOffset(h + 1);
    uint8_t* pixel = data;
    int64_t* integrationPtr = integration.data() + w + 2;
    for (int y = 0; y < h; y++)
    {
        int64_t sum = 0;
        integrationOffset[y] = integration.data() + y * (w + 1);
        for (int x = 0; x < w; x++)
        {
            sum += *pixel;
            if (y == 0)
                *integrationPtr = sum;
            else
                *integrationPtr = sum + *(integrationPtr - (w + 1));

            ++integrationPtr;
            ++pixel;
        }
        ++integrationPtr; // first column should remain zero
        pixel += stride - w;
    }
    integrationOffset[h] = integration.data() + h * (w + 1);

    pixel = data;
    int64_t** offset = integrationOffset.data();
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            int x1 = std::max(x - radius, 0);
            int y1 = std::max(y - radius, 0);
            int x2 = std::min(x + radius, w - 1);
            int y2 = std::min(y + radius, h - 1);
            int area = int(x2 - x1 + 1) * (y2 - y1 + 1);
            int64_t sum = *(offset[y2 + 1] + x2 + 1)
                - *(offset[y2 + 1] + x1)
                - *(offset[y1] + x2 + 1)
                + *(offset[y1] + x1);

            *pixel = std::min<uint8_t>(sum / area, 255);
            ++pixel;
        }
        pixel += stride - w;
    }
}

bool pixelate(CLVideoDecoderOutputPtr frame, const QRect& box, int intensity)
{
    const AVPixFmtDescriptor* descriptor = av_pix_fmt_desc_get((AVPixelFormat)frame->format);
    if (!descriptor)
    {
        NX_ERROR(NX_SCOPE_TAG, "Failed to pixelate frame, invalid pixel format: %1", frame->format);
        return false;
    }

    for (int i = 0; i < QnFfmpegHelper::planeCount(descriptor) && frame->data[i]; ++i)
    {
        int frameHeight = frame->height;
        int frameWidth = frame->width;

        int x = box.x();
        int y = box.y();
        int w = box.width();
        int h = box.height();

        if (QnFfmpegHelper::isChromaPlane(i, descriptor))
        {
            frameHeight >>= descriptor->log2_chroma_h;
            frameWidth >>= descriptor->log2_chroma_w;
            x >>= descriptor->log2_chroma_w;
            y >>= descriptor->log2_chroma_h;
            w >>= descriptor->log2_chroma_w;
            h >>= descriptor->log2_chroma_h;
        }
        y = std::max(y, 0);
        x = std::max(x, 0);
        w = std::min(w, frameWidth - x);
        h = std::min(h, frameHeight - y);

        blur(frame->data[i] + y * frame->linesize[i] + x, w, h, frame->linesize[i], intensity);
    }
    return true;
}

} // namespace

void PixelationImageFilter::pixelateImage(QImage* image, QList<QRect> m_blurRectangles, double intensity)
{
    CLVideoDecoderOutputPtr frame(new CLVideoDecoderOutput(image->convertToFormat(QImage::Format_RGB32)));
    NX_ASSERT(frame, "Failed to convert image to frame");
    int intens = int(intensity * kIntensityToInt);
    for (const auto box: m_blurRectangles)
    {
        pixelate(frame, box, intens);
    }
    *image = frame->toImage();
}

PixelationImageFilter::PixelationImageFilter(const nx::vms::api::PixelationSettings& settings):
    m_settings(settings)
{
}

PixelationImageFilter::~PixelationImageFilter()
{
}

void PixelationImageFilter::setMetadata(const std::list<QnConstAbstractCompressedMetadataPtr>& metadata)
{
    m_metadata = metadata;
}

CLVideoDecoderOutputPtr PixelationImageFilter::updateImage(
    const CLVideoDecoderOutputPtr& frame)
{
    if (m_metadata.empty() || (m_settings.objectTypeIds.empty() && !m_settings.isAllObjectTypes))
        return frame;

    int w = frame->width;
    int h = frame->height;
    CLVideoDecoderOutputPtr result(new CLVideoDecoderOutput());
    result->copyFrom(frame.get());
    for (const auto& metadata: m_metadata)
    {
        if (const auto objectDataPacket = objectDataPacketFromMetadata(metadata))
        {
            for (const auto& objectMetadata: objectDataPacket->objectMetadataList)
            {
                if (m_settings.isAllObjectTypes
                    || m_settings.objectTypeIds.contains(objectMetadata.typeId))
                {
                    QRect box = { (int)(objectMetadata.boundingBox.x() * w),
                        (int)(objectMetadata.boundingBox.y() * h),
                        (int)(objectMetadata.boundingBox.width() * w),
                        (int)(objectMetadata.boundingBox.height() * h) };

                    pixelate(result, box, (int)(m_settings.intensity * kIntensityToInt));
                }
            }
        }
    }
    return result;
}

} // nx::core::transcoding
