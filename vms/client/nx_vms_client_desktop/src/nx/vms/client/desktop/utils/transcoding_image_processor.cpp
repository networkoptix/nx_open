// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "transcoding_image_processor.h"

#include <transcoding/filters/abstract_image_filter.h>
#include <utils/media/frame_info.h>
#include <core/resource/media_resource.h>
#include <nx/core/transcoding/filters/filter_chain.h>

namespace nx::vms::client::desktop {

class TranscodingImageProcessor::Private
{
public:
    const nx::core::transcoding::FilterChain& filters() const
    {
        return m_filters;
    }

    void setSettings(const nx::core::transcoding::Settings& settings,
        const QnMediaResourcePtr& resource)
    {
        if (!resource)
        {
            m_filters.reset();
            m_sourceSize = QSize();
            return;
        }

        m_filters = nx::core::transcoding::FilterChain(
            settings, resource->getDewarpingParams(), resource->getVideoLayout());
        m_sourceSize = QSize();
    }

    nx::core::transcoding::FilterChain ensureFilters(const QSize& sourceSize)
    {
        if (m_sourceSize != sourceSize)
        {
            // Additional check if the new state is less 'empty'.
            // Sometimes we change sizes from (-1;-1) to (0;0).
            if (!sourceSize.isEmpty())
            {
                m_filters.reset();
                // Image is already combined, so video layout must be ignored.
                m_filters.prepareForImage(sourceSize);
                m_sourceSize = sourceSize;
            }
        }

        return m_filters;
    }

private:
    nx::core::transcoding::FilterChain m_filters;

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
    const nx::core::transcoding::Settings& settings,
    const QnMediaResourcePtr& resource)
{
    d->setSettings(settings, resource);
    emit updateRequired();
}

QSize TranscodingImageProcessor::process(const QSize& sourceSize) const
{
    if (!d->filters().isImageTranscodingRequired(sourceSize))
        return sourceSize;

    const auto filters = d->ensureFilters(sourceSize);
    return filters.apply(sourceSize);
}

QImage TranscodingImageProcessor::process(const QImage& sourceImage) const
{
    if (sourceImage.isNull())
        return QImage();

    const auto imageSize = sourceImage.size();
    if (!d->filters().isImageTranscodingRequired(imageSize))
        return sourceImage;

    const auto filters = d->ensureFilters(imageSize);
    if (!filters.isReady())
        return sourceImage;

    QSharedPointer<CLVideoDecoderOutput> frame(new CLVideoDecoderOutput(sourceImage));
    frame = filters.apply(frame);
    return frame ? frame->toImage() : QImage();
}

} // namespace nx::vms::client::desktop
