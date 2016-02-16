#pragma once

#include <QtWidgets/QWidget>

inline void cancelDrag(QWidget *w)
{
    Q_ASSERT_X(w, Q_FUNC_INFO, "Null widget here may lead to interface stuck if drag is in process");
    if (!w)
        return;
    w->grabMouse();
    w->releaseMouse();
}
