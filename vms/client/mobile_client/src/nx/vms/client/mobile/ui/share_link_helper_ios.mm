// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "share_link_helper.h"

#include <QtCore/QUrl>

#import <UIKit/UIKit.h>

namespace nx::vms::client::mobile {

void shareLink(const QUrl& link)
{
    UIViewController* controller =
        []()
        {
            UIViewController *root = UIApplication.sharedApplication.keyWindow.rootViewController;
            while (root.presentedViewController) {
                root = root.presentedViewController;
            }
            return root;
        }();

    if (!controller)
        return;

    UIActivityViewController *activity = [[UIActivityViewController alloc]
        initWithActivityItems:@[link.toNSURL()]
        applicationActivities:nil];

    // iPad specific sheet placing code.
    if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad)
    {
        activity.popoverPresentationController.sourceView = controller.view;
        activity.popoverPresentationController.sourceRect = CGRectMake(
            controller.view.bounds.size.width/2,
            controller.view.bounds.size.height/2,
            1.0,
            1.0);
    }

    [controller presentViewController:activity animated:YES completion:nil];
}

} // namespace nx::vms::client::mobile
