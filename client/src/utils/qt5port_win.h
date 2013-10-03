#ifndef QT5PORT_WINDOWS_SPECIFIC_H
#define QT5PORT_WINDOWS_SPECIFIC_H

#include <QtGlobal>

/* Qt5 windows-platform-dependent functions that are to be implemented. */

#ifdef Q_OS_WIN

#include "windef.h"

HWND widToHwnd(WId id);

#endif


#endif // QT5PORT_WINDOWS_SPECIFIC_H
