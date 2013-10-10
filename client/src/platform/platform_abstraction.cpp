#include "platform_abstraction.h"

#include <QtCore/QCoreApplication>

#include <utils/common/warnings.h>

#if defined(Q_OS_WIN)
#   include "images/images_win.h"
#   define QnImagesImpl QnWindowsImages
#elif defined(Q_OS_LINUX)
#   include "images/images_linux.h"
#   define QnImagesImpl QnLinuxImages
#else
#   include "images/images_generic.h"
#   define QnImagesImpl QnGenericImages
#endif

QnPlatformAbstraction::QnPlatformAbstraction(QObject *parent):
    base_type(parent)
{
    if(!qApp)
        qnWarning("QApplication instance must be created before a QnPlatformAbstraction.");

    m_images = new QnImagesImpl(this);
}

QnPlatformAbstraction::~QnPlatformAbstraction() {
    return;
}
