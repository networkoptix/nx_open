#ifndef WMCTRL_H
#define WMCTRL_H

#include <qglobal.h>

/**
 * Executes window manager control command. Supported only in X11 now.
 */
int execute_wmctrl(QString cmd);

#endif // WMCTRL_H
