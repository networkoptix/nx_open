
#if !defined(_WIN32) && !(defined(Q_OS_LINUX) && !defined(__arm__))

#include "platform_images.h"

#include <QtGui/QCursor>
#include <QtGui/QPixmap>


QCursor QnPlatformImages::bitmapCursor(Qt::CursorShape shape) const
{
    return QCursor(shape);
}

#endif
