#include "transcoding_image_processor.h"

#include <transcoding/filters/abstract_image_filter.h>
#include <transcoding/filters/filter_helper.h>
#include <utils/media/frame_info.h>

namespace nx {
namespace client {
namespace desktop {

TranscodingImageProcessor::TranscodingImageProcessor(QObject* parent):
    base_type(parent)
{
}

TranscodingImageProcessor::~TranscodingImageProcessor()
{
}

void TranscodingImageProcessor::setTranscodingSettings(
    const nx::core::transcoding::LegacyTranscodingSettings& settings)
{
    m_settings = settings;
    emit updateRequired();
}

QSize TranscodingImageProcessor::process(const QSize& sourceSize) const
{
    if (m_settings.resource.isNull() || m_settings.isEmpty())
        return sourceSize;

    QnAbstractImageFilterList filters = QnImageFilterHelper::createFilterChain(
        m_settings, sourceSize);

    QSize size(sourceSize);

    for (const auto& filter: filters)
        size = filter->updatedResolution(size);

    return size;
}

QImage TranscodingImageProcessor::process(const QImage& sourceImage) const
{
    if (sourceImage.isNull())
        return QImage();

    if (m_settings.resource.isNull() || m_settings.isEmpty())
        return sourceImage;

    QnAbstractImageFilterList filters = QnImageFilterHelper::createFilterChain(
        m_settings, sourceImage.size());

    if (filters.empty())
        return sourceImage;

    QSharedPointer<CLVideoDecoderOutput> frame(new CLVideoDecoderOutput(sourceImage));
    for (const auto& filter: filters)
    {
        frame = filter->updateImage(frame);
        if (!frame)
            break;
    }

    return frame ? frame->toImage() : QImage();
}

} // namespace desktop
} // namespace client
} // namespace nx
