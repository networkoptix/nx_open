#include "image_provider.h"

#include <utils/common/delayed.h>

QnImageProvider::QnImageProvider(QObject* parent):
    QObject(parent)
{

}

void QnImageProvider::loadAsync()
{
    doLoadAsync();
}

QnBasicImageProvider::QnBasicImageProvider(const QImage& image, QObject* parent):
    QnImageProvider(parent),
    m_image(image)
{
}

QImage QnBasicImageProvider::image() const
{
    return m_image;
}

void QnBasicImageProvider::doLoadAsync()
{
    executeDelayedParented([this] { emit imageChanged(m_image); }, kDefaultDelay, this);
}
