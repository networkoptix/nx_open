#include "qt5port_windows_specific.h"

HWND widToHwnd(WId id) {
    return reinterpret_cast<HWND>(id);
}



