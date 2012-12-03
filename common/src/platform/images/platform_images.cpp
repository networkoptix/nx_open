#include "platform_images.h"

#if defined Q_OS_WIN
#   include "windows_images.h"
#elif defined Q_WS_X11
#   include "x11_images.h"
#else
#   include "generic_images.h"
#endif

QnPlatformImages *QnPlatformImages::newInstance(QObject *parent) {
#if defined Q_OS_WIN
    return new QnWindowsImages(parent);
#elif defined Q_WS_X11
    return new QnX11Images(parent);
#else
    return new QnGenericImages(parent);
#endif
}


