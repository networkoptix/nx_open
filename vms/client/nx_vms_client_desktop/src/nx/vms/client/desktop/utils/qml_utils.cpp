// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qml_utils.h"

#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QQuickRenderControl>

namespace nx::vms::client::desktop {

QWindow* QmlUtils::renderWindow(QQuickWindow* internalWindow)
{
    const auto result = QQuickRenderControl::renderWindowFor(internalWindow);
    return result ? result : internalWindow;
}

QWindow* QmlUtils::renderWindow(QQuickItem* item)
{
    return renderWindow(item ? item->window() : nullptr);
}

} // namespace nx::vms::client::desktop
