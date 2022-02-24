// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "proxy_image_result.h"

#include <nx/utils/log/assert.h>

namespace nx::vms::client::core {

ProxyImageResult::ProxyImageResult(AsyncImageResult* source, QObject* parent):
    base_type(parent)
{
    if (!NX_ASSERT(source))
    {
        setImage({});
    }
    else if (source->isReady())
    {
        setImage(source->image());
    }
    else
    {
        connect(source, &AsyncImageResult::ready, this,
            [this, source]() { setImage(source->image()); });

        connect(source, &QObject::destroyed, this,
            [this]()
            {
                if (!isReady())
                    setImage({});
            });
    }
}

ProxyImageResult::ProxyImageResult(std::shared_ptr<AsyncImageResult> source, QObject* parent):
    base_type(parent)
{
    if (!NX_ASSERT(source))
    {
        setImage({});
    }
    else if (source->isReady())
    {
        setImage(source->image());
    }
    else
    {
        connect(source.get(), &AsyncImageResult::ready, this,
            [this, source]() { setImage(source->image()); });
    }
}

} // namespace nx::vms::client::core
