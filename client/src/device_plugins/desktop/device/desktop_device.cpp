#ifdef Q_OS_WIN

#include "desktop_device.h"
#include "../streamreader/desktop_stream_reader.h"


CLDesktopDevice::CLDesktopDevice(int index)
{
    m_flags |= local_live_cam;
    QString t = QLatin1String("Desktop") + QString::number(index+1);

    setUrl(t);
    setName(t);
}

CLDesktopDevice::~CLDesktopDevice()
{

}

QString CLDesktopDevice::toString() const
{
    return getUniqueId();
}

QnAbstractStreamDataProvider* CLDesktopDevice::createDataProviderInternal(ConnectionRole /*role*/)
{
    return new CLDesktopStreamreader(toSharedPointer());
}

bool CLDesktopDevice::unknownDevice() const
{
    return false;
}


QString CLDesktopDevice::getUniqueId() const
{
    return getUrl();
}

#endif // Q_OS_WIN
