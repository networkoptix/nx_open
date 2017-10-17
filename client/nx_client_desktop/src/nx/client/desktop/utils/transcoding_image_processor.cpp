#include "transcoding_image_processor.h"

#include <transcoding/filters/abstract_image_filter.h>
#include <transcoding/filters/filter_helper.h>
#include <utils/media/frame_info.h>

namespace nx {
namespace client {
namespace desktop {

class TranscodingImageProcessor::Private
{
public:
    const nx::core::transcoding::LegacyTranscodingSettings& settings() const
    {
        return m_settings;
    }

    void setSettings(const nx::core::transcoding::LegacyTranscodingSettings& settings)
    {
        m_settings = settings;
        m_sourceSize = QSize();
    }

    QnAbstractImageFilterList ensureFilters(const QSize& sourceSize)
    {
        if (m_sourceSize != sourceSize)
        {
            m_filters = QnImageFilterHelper::createFilterChain(m_settings, sourceSize);
            m_sourceSize = sourceSize;
        }

        return m_filters;
    }

private:
    nx::core::transcoding::LegacyTranscodingSettings m_settings;
    QnAbstractImageFilterList m_filters;
    QSize m_sourceSize;
};

TranscodingImageProcessor::TranscodingImageProcessor(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

TranscodingImageProcessor::~TranscodingImageProcessor()
{
}

void TranscodingImageProcessor::setTranscodingSettings(
    const nx::core::transcoding::LegacyTranscodingSettings& settings)
{
    d->setSettings(settings);
    emit updateRequired();
}

QSize TranscodingImageProcessor::process(const QSize& sourceSize) const
{
    if (d->settings().resource.isNull() || d->settings().isEmpty())
        return sourceSize;

    QSize size(sourceSize);

    for (const auto& filter: d->ensureFilters(sourceSize))
        size = filter->updatedResolution(size);

    return size;
}

QImage TranscodingImageProcessor::process(const QImage& sourceImage) const
{
    if (sourceImage.isNull())
        return QImage();

    if (d->settings().resource.isNull() || d->settings().isEmpty())
        return sourceImage;

    const auto filters = d->ensureFilters(sourceImage.size());
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
