#include "desktop_device.h"

#ifdef Q_OS_WIN
#  include "../streamreader/desktop_stream_reader.h"
#endif

CLDesktopDevice::CLDesktopDevice(int index)
    : QnResource()
{
    addFlags(local_live_cam);

    const QString name = QLatin1String("Desktop") + QString::number(index + 1);
    setName(name);
    setUrl(name);
}

QString CLDesktopDevice::toString() const
{
    return getUniqueId();
}

QnAbstractStreamDataProvider* CLDesktopDevice::createDataProviderInternal(ConnectionRole /*role*/)
{
#ifdef Q_OS_WIN
    return new CLDesktopStreamreader(toSharedPointer());
#else
    return 0;
#endif
}

bool CLDesktopDevice::unknownDevice() const
{
    return false;
}

QString CLDesktopDevice::getUniqueId() const
{
    return getUrl();
}
