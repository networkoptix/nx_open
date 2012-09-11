#ifndef WMCTRL_H
#define WMCTRL_H

#include <qglobal.h>

#ifdef Q_WS_X11
int execute_wmctrl(QString cmd);
#endif

#endif // WMCTRL_H
