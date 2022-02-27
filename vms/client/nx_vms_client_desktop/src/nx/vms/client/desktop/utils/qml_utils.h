// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

class QQuickItem;
class QQuickWindow;
class QWindow;

namespace nx::vms::client::desktop {

class QmlUtils
{
public:
    static QWindow* renderWindow(QQuickWindow* internalWindow);
    static QWindow* renderWindow(QQuickItem* item);
};

} // namespace nx::vms::client::desktop
