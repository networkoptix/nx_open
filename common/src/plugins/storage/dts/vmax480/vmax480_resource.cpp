#include "vmax480_resource.h"
#include "vmax480_live_reader.h"

const char* QnPlVmax480Resource::MANUFACTURE = "VMAX";

QnPlVmax480Resource::QnPlVmax480Resource()
{

}

int QnPlVmax480Resource::getMaxFps() 
{
    return 30;
}

bool QnPlVmax480Resource::isResourceAccessible() 
{
    return true;
}

bool QnPlVmax480Resource::updateMACAddress() 
{
    return true;
}

QString QnPlVmax480Resource::manufacture() const 
{
    return QLatin1String(MANUFACTURE);
}

void QnPlVmax480Resource::setIframeDistance(int frames, int timems) 
{

}

QString QnPlVmax480Resource::getHostAddress() const 
{
    return QString();
}

bool QnPlVmax480Resource::setHostAddress(const QString &ip, QnDomain domain) 
{
    QUrl url(getUrl());
    url.setHost(ip);
    setUrl(url.toString());
    return true;
}

int QnPlVmax480Resource::channelNum() const
{
    //vmax://ip:videoport:event:channel
    QString url = getUrl();

    QStringList lst = url.split(QLatin1Char(':'));
    if (lst.size() < 5)
        return 0;

    return lst[4].toInt();
}

int QnPlVmax480Resource::videoPort() const
{
    //vmax://ip:videoport:event:channel
    QString url = getUrl();

    QStringList lst = url.split(QLatin1Char(':'));
    if (lst.size() < 5)
        return 0;

    return lst[2].toInt();

}

int QnPlVmax480Resource::eventPort() const
{
    //vmax://ip:videoport:event:channel
    QString url = getUrl();

    QStringList lst = url.split(QLatin1Char(':'));
    if (lst.size() < 5)
        return 0;

    return lst[3].toInt();
    
}

bool QnPlVmax480Resource::shoudResolveConflicts() const 
{
    return false;
}

QnAbstractStreamDataProvider* QnPlVmax480Resource::createLiveDataProvider()
{
    return new QnVMax480LiveProvider(toSharedPointer());
}

void QnPlVmax480Resource::setCropingPhysical(QRect croping)
{

}

bool QnPlVmax480Resource::initInternal()
{

    Qn::CameraCapabilities addFlags = Qn::PrimaryStreamSoftMotionCapability;
    setCameraCapabilities(getCameraCapabilities() | addFlags);
    save();

    return true;
}
