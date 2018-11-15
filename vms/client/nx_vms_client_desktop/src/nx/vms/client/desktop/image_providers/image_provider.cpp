#include "image_provider.h"

#include <client/client_globals.h>

#include <utils/common/delayed.h>

namespace nx::vms::client::desktop {

ImageProvider::ImageProvider(QObject* parent):
    QObject(parent)
{
}

void ImageProvider::loadAsync()
{
    doLoadAsync();
}

BasicImageProvider::BasicImageProvider(const QImage& image, QObject* parent):
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

void BasicImageProvider::doLoadAsync()
{
    executeDelayedParented(
        [this]
        {
            emit imageChanged(m_image);
            emit statusChanged(Qn::ThumbnailStatus::Loaded);
        }, this);
}

} // namespace nx::vms::client::desktop
