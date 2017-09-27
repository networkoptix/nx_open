#pragma once

#if defined(ENABLE_DATA_PROVIDERS)

#include <core/resource/resource_fwd.h>

#include <nx/core/transcoding/filters/transcoding_settings.h>

#include <transcoding/filters/abstract_image_filter.h>

class CLVideoDecoderOutput;
using CLVideoDecoderOutputPtr = QSharedPointer<CLVideoDecoderOutput>;

namespace nx {
namespace core {
namespace transcoding {

class FilterChain: public QnAbstractImageFilterList
{
public:
    const static QSize kDefaultResolutionLimit;

    FilterChain() = default;
    explicit FilterChain(const Settings& settings);
    FilterChain(const FilterChain&) = default;
    FilterChain& operator=(const FilterChain&) = default;

    void prepare(const QnMediaResourcePtr& resource,
        const QSize& srcResolution,
        const QSize& resolutionLimit = kDefaultResolutionLimit);

    bool isTranscodingRequired(const QnMediaResourcePtr& resource) const;

    bool isDownscaleRequired(const QSize& srcResolution) const;

    bool isReady() const;

    QSize apply(const QSize& resolution) const;
    CLVideoDecoderOutputPtr apply(const CLVideoDecoderOutputPtr& source) const;

private:
    bool m_ready = false;
    Settings m_settings;
};

} // namespace transcoding
} // namespace core
} // namespace nx

#endif // ENABLE_DATA_PROVIDERS
