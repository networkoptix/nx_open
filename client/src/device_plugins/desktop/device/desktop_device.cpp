#include "desktop_device.h"
#include "../streamreader/desktop_stream_reader.h"

QnDesktopDevice::QnDesktopDevice(int index): QnResource() {
    addFlags(local_live_cam);

    const QString name = QLatin1String("Desktop") + QString::number(index + 1);
    setName(name);
    setUrl(name);
}

QString QnDesktopDevice::toString() const {
    return getUniqueId();
}

QnAbstractStreamDataProvider *QnDesktopDevice::createDataProviderInternal(ConnectionRole /*role*/) {
#ifdef Q_OS_WIN
    return new QnDesktopStreamreader(toSharedPointer());
#else
    return 0;
#endif
}

QString QnDesktopDevice::getUniqueId() const {
    return getUrl();
}
