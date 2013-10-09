#include "platform_abstraction.h"

#include <QtWidgets/QApplication>

#include <utils/common/warnings.h>

#if defined(Q_OS_WIN)
#   include "images/images_win.h"
#elif defined(Q_OS_LINUX)
#   include "images/images_unix.h"
#else
#   include "images/images_generic.h"
#endif

QnPlatformAbstraction::QnPlatformAbstraction(QObject *parent):
    base_type(parent)
{
    if(!qApp)
        qnWarning("QApplication instance must be created before a QnPlatformAbstraction.");

#if defined(Q_OS_WIN)
    m_images = new QnWindowsImages(this);
#elif defined(Q_OS_LINUX)
    m_images = new QnUnixImages(this);
#else
    m_images = new QnGenericImages(this);
#endif
}

QnPlatformAbstraction::~QnPlatformAbstraction() {
    return;
}
