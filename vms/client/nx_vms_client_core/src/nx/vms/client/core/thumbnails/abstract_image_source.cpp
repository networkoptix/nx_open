// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_image_source.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/log/format.h>

#include "thumbnail_image_provider.h"

namespace nx::vms::client::core {

AbstractImageSource::AbstractImageSource(
    ThumbnailImageProvider* provider,
    const QString& id)
    :
    id(id),
    m_provider(provider)
{
    if (NX_ASSERT(m_provider))
        m_provider->addSource(this);
}

AbstractImageSource::~AbstractImageSource()
{
    if (m_provider)
        m_provider->removeSource(this);
}

QUrl AbstractImageSource::makeUrl(const QString& imageId) const
{
    return QString(nx::format("image://%1/%2/%3").args(ThumbnailImageProvider::id, id, imageId));
}

} // namespace nx::vms::client::core
