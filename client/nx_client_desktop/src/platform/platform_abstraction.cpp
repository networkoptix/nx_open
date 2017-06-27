#include "platform_abstraction.h"

#include <QtCore/QCoreApplication>

#include <utils/common/warnings.h>

#if defined(Q_OS_WIN)
#   include "images/images_win.h"
#   define QnImagesImpl QnWindowsImages

#   include "shortcuts/shortcuts_win.h"
#   define QnShortcutsImpl QnWindowsShortcuts
#elif defined(Q_OS_LINUX)
#   include "images/images_linux.h"
#   define QnImagesImpl QnLinuxImages

#   include "shortcuts/shortcuts_linux.h"
#   define QnShortcutsImpl QnLinuxShortcuts
#else
#   include "images/images_generic.h"
#   define QnImagesImpl QnGenericImages

#   include "shortcuts/shortcuts_mac.h"
#   define QnShortcutsImpl QnMacShortcuts
#endif

QnPlatformAbstraction::QnPlatformAbstraction(QObject *parent):
    base_type(parent)
{
    if(!qApp)
        qnWarning("QApplication instance must be created before a QnPlatformAbstraction.");

    m_images = new QnImagesImpl(this);
    m_shortcuts = new QnShortcutsImpl(this);
}

QnPlatformAbstraction::~QnPlatformAbstraction() {
    return;
}
