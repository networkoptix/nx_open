// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "transcoding_image_processor.h"

#include <core/resource/media_resource.h>
#include <nx/core/transcoding/filters/filter_chain.h>
#include <nx/media/ffmpeg/frame_info.h>
#include <transcoding/filters/abstract_image_filter.h>

namespace nx::vms::client::desktop {

class TranscodingImageProcessor::Private
{
public:
    const nx::core::transcoding::FilterChain* filters() const
    {
        return m_filters.get();
    }

    void setSettings(const nx::core::transcoding::Settings& settings)
    {
        m_filters = std::make_unique<nx::core::transcoding::FilterChain>(settings);
        m_sourceSize = QSize();
    }

    void ensureFilters(const QSize& sourceSize)
    {
        if (!m_filters)
            return;

        if (m_sourceSize != sourceSize)
        {
            // Additional check if the new state is less 'empty'.
            // Sometimes we change sizes from (-1;-1) to (0;0).
            if (!sourceSize.isEmpty())
            {
                m_filters->reset();
                // Image is already combined, so video layout must be ignored.
                m_filters->prepareForImage(sourceSize);
                m_sourceSize = sourceSize;
            }
        }
    }

private:
    nx::core::transcoding::FilterChainPtr m_filters;

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
    const nx::core::transcoding::Settings& settings)
{
    d->setSettings(settings);
    emit updateRequired();
}

QSize TranscodingImageProcessor::process(const QSize& sourceSize) const
{
    if (!d->filters() || !d->filters()->isImageTranscodingRequired(sourceSize))
        return sourceSize;

    d->ensureFilters(sourceSize);
    return d->filters()->apply(sourceSize);
}

QImage TranscodingImageProcessor::process(const QImage& sourceImage) const
{
    if (sourceImage.isNull())
        return QImage();

    if (!d->filters())
        return sourceImage;

    const auto imageSize = sourceImage.size();
    if (!d->filters()->isImageTranscodingRequired(imageSize))
        return sourceImage;

    d->ensureFilters(imageSize);
    if (!d->filters()->isReady())
        return sourceImage;

    QSharedPointer<CLVideoDecoderOutput> frame(new CLVideoDecoderOutput(sourceImage));
    frame = d->filters()->apply(frame);
    return frame ? frame->toImage() : QImage();
}

} // namespace nx::vms::client::desktop
