// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "generic_image_store.h"

namespace nx::vms::client::core {

QImage GenericImageStore::image(const QString& imageId) const
{
    return m_images.value(imageId);
}

void GenericImageStore::addImage(const QString& imageId, const QImage& image)
{
    m_images[imageId] = image;
}

QString GenericImageStore::addImage(const QImage& image)
{
    const auto imageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    addImage(imageId, image);
    return imageId;
}

void GenericImageStore::removeImage(const QString& imageId)
{
    m_images.remove(imageId);
}

} // namespace nx::vms::client::core
