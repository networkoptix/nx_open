#include "qt5port_win.h"

HWND widToHwnd(WId id) {
    return reinterpret_cast<HWND>(id);
}



