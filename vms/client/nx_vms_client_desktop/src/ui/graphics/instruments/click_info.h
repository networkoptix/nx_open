// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QInternal>
#include <QtCore/QMetaType>
#include <QtCore/QPoint>

struct ClickInfo
{
    QPointF scenePos;
    Qt::MouseButton button = Qt::MouseButton::NoButton;
    Qt::KeyboardModifiers modifiers = {};
};
Q_DECLARE_METATYPE(ClickInfo)
