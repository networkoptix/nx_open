// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <nx/utils/log/assert.h>

inline void cancelDrag(QWidget *w)
{
    NX_ASSERT(w, "Null widget here may lead to interface stuck if drag is in process");
    if (!w)
        return;
    w->grabMouse();
    w->releaseMouse();
}
