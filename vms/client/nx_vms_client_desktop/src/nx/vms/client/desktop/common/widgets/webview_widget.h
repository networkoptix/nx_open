// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtQuickWidgets/QQuickWidget>

#include "webview_controller.h"

class QQmlEngine;

namespace nx::vms::client::desktop {

/**
 * A replacement for standard QWebEngineView, but based on QML WebView.
 * Allows to customize various parameters in one place via `WebViewController` class
 * (user agent, context menu content, link delegation, ignore ssl errors etc.)
 */
class NX_VMS_CLIENT_DESKTOP_API WebViewWidget: public QQuickWidget
{
    Q_OBJECT
    using base_type = QQuickWidget;

public:
    WebViewWidget(QWidget* parent, QQmlEngine* engine = nullptr);
    virtual ~WebViewWidget() override;

    WebViewController* controller() const;

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
