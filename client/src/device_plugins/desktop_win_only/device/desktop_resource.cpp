#include "desktop_resource.h"
#include "../streamreader/desktop_stream_reader.h"

QnDesktopResource::QnDesktopResource(int index): QnResource() {
    addFlags(local_live_cam);

    const QString name = QLatin1String("Desktop") + QString::number(index + 1);
    setName(name);
    setUrl(name);
}

QString QnDesktopResource::toString() const {
    return getUniqueId();
}

QnAbstractStreamDataProvider *QnDesktopResource::createDataProviderInternal(ConnectionRole /*role*/) {
#ifdef Q_OS_WIN
    return new QnDesktopStreamreader(toSharedPointer());
#else
    return 0;
#endif
}

QString QnDesktopResource::getUniqueId() const {
    return getUrl();
}
