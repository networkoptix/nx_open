#include "image_provider.h"

#include <client/client_globals.h>

#include <utils/common/delayed.h>

namespace nx::vms::client::desktop {

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

Qn::ThumbnailStatus BasicImageProvider::status() const
{
    return Qn::ThumbnailStatus::Loaded;
}

bool BasicImageProvider::tryLoad()
{
    emit imageChanged(m_image);
    emit statusChanged(Qn::ThumbnailStatus::Loaded);
    return true;
}

void BasicImageProvider::doLoadAsync()
{
    executeLater([this]() { tryLoad(); }, this);
}

} // namespace nx::vms::client::desktop
