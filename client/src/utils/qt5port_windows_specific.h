#ifndef QT5PORT_WINDOWS_SPECIFIC_H
#define QT5PORT_WINDOWS_SPECIFIC_H

#include <QtGlobal>

/** Qt5 windows-platform-dependent functions that are to be implemented. */

#ifdef Q_OS_WIN

#include <QtGui/QPixmap>

#include "windef.h"


QPixmap pixmapFromWinHICON(HICON handle);
HWND widToHwnd(WId id);
WId hwndToWid(HWND id);

#endif


#endif // QT5PORT_WINDOWS_SPECIFIC_H
