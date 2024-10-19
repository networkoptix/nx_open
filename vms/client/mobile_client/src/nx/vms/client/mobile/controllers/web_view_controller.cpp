// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "web_view_controller.h"

#if !defined(Q_OS_IOS)

#include <QtGui/QDesktopServices>

namespace nx::vms::client::mobile {

struct WebViewController::Private {};

WebViewController::WebViewController(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

WebViewController::~WebViewController()
{
}

void WebViewController::openUrl(const QUrl& url)
{
    QDesktopServices::openUrl(url);
}

void WebViewController::closeWindow()
{
}

} // namespace nx::vms::client::mobile

#endif
