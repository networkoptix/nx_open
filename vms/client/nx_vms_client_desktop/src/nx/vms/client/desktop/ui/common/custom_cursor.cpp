// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "custom_cursor.h"

#include <QtCore/QList>
#include <QtGui/QGuiApplication>
#include <QtQml/QtQml>
#include <QtQuick/QQuickItem>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

CustomCursorAttached::CustomCursorAttached(QObject* parent):
    QObject(parent)
{
}

QCursor CustomCursorAttached::cursor() const
{
    const auto item = qobject_cast<QQuickItem*>(parent());
    return NX_ASSERT(item) ? item->cursor() : QCursor();
}

void CustomCursorAttached::setCursor(QCursor value)
{
    if (auto item = qobject_cast<QQuickItem*>(parent()); NX_ASSERT(item))
        item->setCursor(value);
}

void CustomCursorAttached::unsetCursor()
{
    if (auto item = qobject_cast<QQuickItem*>(parent()); NX_ASSERT(item))
        item->unsetCursor();
}

CustomCursorAttached* CustomCursor::qmlAttachedProperties(QObject* parent)
{
    return new CustomCursorAttached(parent);
}

void CustomCursor::registerQmlType()
{
    qmlRegisterType<CustomCursor>("nx.vms.client.desktop", 1, 0, "CustomCursor");
}

} // namespace nx::vms::client::desktop
