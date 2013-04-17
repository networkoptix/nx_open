#include "platform_images.h"

#include "images_win.h"
#include "images_unix.h"
#include "generic_images.h"

QnPlatformImages *QnPlatformImages::newInstance(QObject *parent) {
#if defined Q_OS_WIN
    return new QnWindowsImages(parent);
#elif defined Q_OS_LINUX
    return new QnX11Images(parent);
#else
    return new QnGenericImages(parent);
#endif
}


