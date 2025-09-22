// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "pixelation_image_filter.h"

#include <optional>

#include <analytics/common/object_metadata.h>
#include <nx/fusion/serialization/ubjson.h>
#include <nx/media/ffmpeg/frame_info.h>
#include <nx/vms/common/pixelation/pixelation.h>
#include <utils/common/delayed.h>

namespace nx::core::transcoding {
namespace {

std::optional<nx::common::metadata::ObjectMetadataPacket> objectDataPacketFromMetadata(
    const QnAbstractCompressedMetadataPtr& metadata)
{
    const auto compressedMetadata = std::dynamic_pointer_cast<QnCompressedMetadata>(metadata);
    if (!compressedMetadata)
        return {};

    return QnUbjson::deserialized<nx::common::metadata::ObjectMetadataPacket>(
        QByteArray::fromRawData(compressedMetadata->data(), int(compressedMetadata->dataSize())));
}

} // namespace

PixelationImageFilter::PixelationImageFilter(const nx::vms::api::PixelationSettings& settings):
    m_settings(settings)
{
}

PixelationImageFilter::~PixelationImageFilter()
{
    // Deleting in the thread it was created.
    executeInThread(m_pixelation->thread(), [pixelation = m_pixelation]() {});
}

void PixelationImageFilter::setMetadata(const QnAbstractCompressedMetadataPtr& metadata)
{
    m_metadata = metadata;
}

CLVideoDecoderOutputPtr PixelationImageFilter::updateImage(
    const CLVideoDecoderOutputPtr& frame)
{
    // Creating in the thread it is used.
    if (!m_pixelation)
        m_pixelation = std::make_shared<nx::vms::common::pixelation::Pixelation>();

    if (!m_metadata || (m_settings.objectTypeIds.empty() && !m_settings.isAllObjectTypes))
        return frame;

    if (const auto objectDataPacket = objectDataPacketFromMetadata(m_metadata))
    {
        QVector<QRectF> boundingBoxes;
        for (const auto& objectMetadata: objectDataPacket->objectMetadataList)
        {
            if (m_settings.isAllObjectTypes
                || m_settings.objectTypeIds.contains(objectMetadata.typeId))
            {
                boundingBoxes.push_back(objectMetadata.boundingBox);
            }
        }

        setImage(m_pixelation->pixelate(frame->toImage(), boundingBoxes, m_settings.intensity));
        return PaintImageFilter::updateImage(frame);
    }

    return frame;
}

} // nx::core::transcoding
