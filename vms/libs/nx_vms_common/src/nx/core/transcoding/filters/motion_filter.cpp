// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "motion_filter.h"

#include <limits>

#include <QtGui/QImage>
#include <QtGui/QPainter>

#include <nx/media/ffmpeg/frame_info.h>
#include <nx/vms/common/utils/object_painter_helper.h>

namespace nx::core::transcoding {
namespace {

std::shared_ptr<const QnMetaDataV1> motionDataPacketFromMetadata(
    const QnConstAbstractCompressedMetadataPtr& metadata)
{
    const auto motionMetadata = std::dynamic_pointer_cast<const QnMetaDataV1>(metadata);
    if (!motionMetadata || motionMetadata->dataSize() > std::numeric_limits<int>::max())
        return nullptr;

    return motionMetadata;
}

} // namespace

QSize MotionFilter::updatedResolution(const QSize& sourceSize)
{
    return sourceSize;
}

MotionFilter::MotionFilter(const MotionExportSettings& settings):
    m_settings(settings)
{
}

MotionFilter::~MotionFilter()
{
}

void MotionFilter::setMetadata(const std::vector<QnConstMetaDataV1Ptr>& metadata)
{
    m_metadata = metadata;
}

CLVideoDecoderOutputPtr MotionFilter::updateImage(const CLVideoDecoderOutputPtr& frame)
{
    QImage image = frame->toImage();
    updateImage(image);

    CLVideoDecoderOutputPtr resFrame(new CLVideoDecoderOutput(image, SWS_CS_ITU601));
    resFrame->assignMiscData(frame.get());
    return resFrame;
}

void MotionFilter::updateImage(QImage& image)
{
    const qreal width = image.width();
    const qreal height = image.height();

    QPainter painter(&image);

    if (m_metadata.empty())
    {
        paintMotionGrid(&painter, {0.0, 0.0, width, height}, {});
        return;
    }

    for (auto iter = m_metadata.rbegin(); iter != m_metadata.rend(); ++iter)
    {
        const auto motionDataPacket = motionDataPacketFromMetadata(*iter);
        if (motionDataPacket)
        {
            paintMotionGrid(&painter, {0.0, 0.0, width, height}, motionDataPacket);
            break;
        }
    }
}

void MotionFilter::paintMotionGrid(
    QPainter* painter,
    const QRectF& rect,
    const QnConstMetaDataV1Ptr& motion)
{
    auto gridLines = vms::common::ObjectPainterHelper::calculateMotionGrid(rect, motion);

    painter->translate(rect.topLeft());

    painter->setPen(QPen(m_settings.background, 0.0));
    painter->drawLines(gridLines[0]);

    painter->setPen(QPen(m_settings.foreground, 0.0));
    painter->drawLines(gridLines[1]);
}

} // namespace nx::core::transcoding
