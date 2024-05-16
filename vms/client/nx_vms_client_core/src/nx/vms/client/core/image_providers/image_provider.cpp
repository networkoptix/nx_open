// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "image_provider.h"

#include <utils/common/delayed.h>

namespace nx::vms::client::core {

ImageProvider::ImageProvider(QObject* parent):
    QObject(parent)
{
}

bool ImageProvider::tryLoad()
{
    return false;
}

void ImageProvider::loadAsync()
{
    doLoadAsync();
}

BasicImageProvider::BasicImageProvider(const QImage& image, QObject* parent) :
    ImageProvider(parent),
    m_image(image)
{
}

QImage BasicImageProvider::image() const
{
    return m_image;
}

QSize BasicImageProvider::sizeHint() const
{
    return m_image.size() / m_image.devicePixelRatio();
}

ThumbnailStatus BasicImageProvider::status() const
{
    return ThumbnailStatus::Loaded;
}

bool BasicImageProvider::tryLoad()
{
    emit imageChanged(m_image);
    emit statusChanged(ThumbnailStatus::Loaded);
    return true;
}

void BasicImageProvider::doLoadAsync()
{
    executeLater([this]() { tryLoad(); }, this);
}

} // namespace nx::vms::client::core
