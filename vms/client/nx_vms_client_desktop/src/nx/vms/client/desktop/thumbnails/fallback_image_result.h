// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <memory>

#include <nx/vms/client/core/thumbnails/async_image_result.h>

namespace nx::vms::client::desktop {

class FallbackImageResult: public nx::vms::client::core::AsyncImageResult
{
public:
    using ImageSelector = std::function<QImage(const QImage&, const QImage&)>;

    FallbackImageResult(std::unique_ptr<nx::vms::client::core::AsyncImageResult> source,
        const QImage& fallback,
        ImageSelector selector);

private:
    const std::unique_ptr<nx::vms::client::core::AsyncImageResult> m_source;
    const QImage m_fallback;
    ImageSelector m_selector;
};

} // namespace nx::vms::client::desktop
