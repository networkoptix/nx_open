// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fallback_image_result.h"

#include <nx/utils/log/assert.h>

namespace nx::vms::client::core {

FallbackImageResult::FallbackImageResult(
    std::unique_ptr<AsyncImageResult> source,
    const QImage& fallback,
    ImageSelector selector)
    :
    m_source(std::move(source)),
    m_fallback(fallback),
    m_selector(selector)
{
    if (!m_source)
    {
        setImage(m_fallback);
        return;
    }

    if (!NX_ASSERT(m_selector))
    {
        setImage({});
        return;
    }

    const auto updateImage =
        [this]() { setImage(m_selector(m_source->image(), m_fallback)); };

    if (m_source->isReady())
        updateImage();
    else
        connect(m_source.get(), &AsyncImageResult::ready, this, updateImage);
}

} // namespace nx::vms::client::core
