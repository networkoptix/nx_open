// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "web_view_controller.h"

#include <UIKit/UIKit.h>
#include <SafariServices/SFSafariViewController.h>

#include <QtCore/QSharedPointer>
#include <QtGui/QDesktopServices>

namespace nx::vms::client::mobile {

struct WebViewController::Private
{
    QSharedPointer<SFSafariViewController> safariController;
};

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
    closeWindow();

    if ([SFSafariViewController class] == nil)
    {
        QDesktopServices::openUrl(url);
        return;
    }

    d->safariController.reset([[SFSafariViewController alloc] initWithURL: url.toNSURL()],
        [](SFSafariViewController* pointer)
        {
            [pointer release];
        });

    const auto window = [[UIApplication sharedApplication] keyWindow];
    const auto rootController = [window rootViewController];
    [rootController presentViewController: d->safariController.data()
        animated: YES
        completion: nil];
}

void WebViewController::closeWindow()
{
    if (!d->safariController)
        return;

    [d->safariController.data() dismissViewControllerAnimated: TRUE completion: nil];
    d->safariController.reset();

}

} // namespace nx::vms::client::mobile
