// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webview_widget.h"

#include <nx/vms/client/desktop/application_context.h>

namespace nx::vms::client::desktop {

struct WebViewWidget::Private
{
    QScopedPointer<WebViewController> controller;
};

WebViewWidget::WebViewWidget(QWidget* parent, QQmlEngine* engine):
    base_type(engine ? engine : appContext()->qmlEngine(), parent),
    d(new Private())
{
    d->controller.reset(new WebViewController(this));
    d->controller->loadIn(this);
}

WebViewController* WebViewWidget::controller() const
{
    return d->controller.data();
}

WebViewWidget::~WebViewWidget()
{
}

} // namespace nx::vms::client::desktop
