// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "webview_controller.h"

namespace nx::vms::client::desktop {

class WebViewWidget;

/**
 * Adds busy indicator on top of WebViewWidget.
 */
class WebWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    WebWidget(QWidget* parent = nullptr);

    WebViewController* controller() const;

private:
    WebViewWidget* m_webView = nullptr;
};

} // namespace nx::vms::client::desktop
