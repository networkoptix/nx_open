// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "thumbnail_image_provider.h"

#include <nx/utils/log/assert.h>

#include "abstract_image_source.h"

namespace nx::vms::client::core {

const QString ThumbnailImageProvider::id = "thumbnails";

ThumbnailImageProvider::ThumbnailImageProvider():
    QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage ThumbnailImageProvider::requestImage(
    const QString& id, QSize* size, const QSize& /*requestedSize*/)
{
    const auto parts = id.split('/');
    if (parts.size() < 2)
        return {};

    const auto source = m_sourceById.value(parts.first());
    if (!source)
        return {};

    const auto image = source->image(parts[1]);
    *size = image.size();
    return image;
}

void ThumbnailImageProvider::addSource(AbstractImageSource* source)
{
    if (NX_ASSERT(source && !m_sourceById.contains(source->id)))
        m_sourceById.insert(source->id, source);
}

void ThumbnailImageProvider::removeSource(AbstractImageSource* source)
{
    if (!NX_ASSERT(source))
        return;

    const auto iter = m_sourceById.find(source->id);
    if (iter != m_sourceById.end() && NX_ASSERT(iter.value() == source))
        m_sourceById.erase(iter);
}

} // namespace nx::vms::client::core

template<> nx::vms::client::core::ThumbnailImageProvider*
    Singleton<nx::vms::client::core::ThumbnailImageProvider>::s_instance = nullptr;
