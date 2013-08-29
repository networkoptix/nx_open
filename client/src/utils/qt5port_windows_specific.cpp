#include "qt5port_windows_specific.h"

#ifdef Q_OS_WIN

QPixmap pixmapFromWinHICON(HICON handle) {
    Q_UNUSED(handle);
    return QPixmap();
}

HWND widToHwnd(WId id) {
    return reinterpret_cast<HWND>(id);
}

WId hwndToWid(HWND id) {
    return reinterpret_cast<WId>(id);
}


#endif
