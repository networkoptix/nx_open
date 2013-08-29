
#include "platform_images.h"


QnPlatformImages::QnPlatformImages(QObject *parent)
:
    QObject(parent) 
{
}

QnPlatformImages::~QnPlatformImages()
{
}

QnPlatformImages *QnPlatformImages::newInstance(QObject *parent)
{
    return new QnPlatformImages(parent);
}
