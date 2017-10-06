#include "transcoding_image_processor.h"

#include <transcoding/filters/abstract_image_filter.h>
#include <utils/media/frame_info.h>

namespace nx {
namespace client {
namespace desktop {

class TranscodingImageProcessor::Private
{
public:
    const core::transcoding::FilterChain& filters() const
    {
        return m_filters;
    }

    void setSettings(const nx::core::transcoding::Settings& settings)
    {
        m_filters = core::transcoding::FilterChain(settings);
        m_sourceSize = QSize();
    }

    core::transcoding::FilterChain ensureFilters(const QSize& sourceSize)
    {
        if (m_sourceSize != sourceSize)
        {
            m_filters.reset();
            // Image is already combined, so video layout must be ignored.
            m_filters.prepare(QnConstResourceVideoLayoutPtr(),
                m_mediaDewarpingParams,
                sourceSize);
            m_sourceSize = sourceSize;
        }

        return m_filters;
    }

private:
    QnMediaDewarpingParams m_mediaDewarpingParams;
    core::transcoding::FilterChain m_filters;

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
    const core::transcoding::Settings& settings,
    const QnMediaDewarpingParams& mediaDewarpingParams)
{
    d->setSettings(settings);
    emit updateRequired();
}

QSize TranscodingImageProcessor::process(const QSize& sourceSize) const
{
    if (!d->filters().isTranscodingRequired(QnConstResourceVideoLayoutPtr()))
        return sourceSize;

    const auto filters = d->ensureFilters(sourceSize);
    return filters.apply(sourceSize);
}

QImage TranscodingImageProcessor::process(const QImage& sourceImage) const
{
    if (sourceImage.isNull())
        return QImage();

    if (!d->filters().isTranscodingRequired(QnConstResourceVideoLayoutPtr()))
        return sourceImage;

    const auto filters = d->ensureFilters(sourceImage.size());
    QSharedPointer<CLVideoDecoderOutput> frame(new CLVideoDecoderOutput(sourceImage));
    frame = filters.apply(frame);
    return frame ? frame->toImage() : QImage();
}

} // namespace desktop
} // namespace client
} // namespace nx
