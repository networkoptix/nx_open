// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "proxy_image_result.h"

#include <nx/utils/log/assert.h>

namespace nx::vms::client::core {

ProxyImageResult::ProxyImageResult(AsyncImageResult* source, QObject* parent):
    base_type(parent)
{
    if (!NX_ASSERT(source))
    {
        setError("Invalid operation");
    }
    else if (source->isReady())
    {
        handleSourceReady(source);
    }
    else
    {
        connect(source, &AsyncImageResult::ready, this,
            [this, source]() { handleSourceReady(source); });

        connect(source, &QObject::destroyed, this,
            [this]()
            {
                if (!isReady())
                    setError("Source request cancelled");
            });
    }
}

ProxyImageResult::ProxyImageResult(std::shared_ptr<AsyncImageResult> source, QObject* parent):
    base_type(parent)
{
    if (!NX_ASSERT(source))
    {
        setError("Invalid operation");
    }
    else if (source->isReady())
    {
        handleSourceReady(source.get());
    }
    else
    {
        connect(source.get(), &AsyncImageResult::ready, this,
            [this, source]() { handleSourceReady(source.get()); });
    }
}

void ProxyImageResult::handleSourceReady(AsyncImageResult* source)
{
    if (const auto image = source->image(); !image.isNull())
        setImage(image);
    else
        setError(source->errorString());
}

} // namespace nx::vms::client::core
