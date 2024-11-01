// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <UIKit/UIKit.h>
#include <UIKit/UiApplication.h>
#include <UserNotifications/UserNotifications.h>

#include <QtCore/QByteArray>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>

#include <nx/vms/client/mobile/push_notification/details/token_data_watcher.h>
#include <nx/utils/log/log.h>

@interface QIOSApplicationDelegate
@end

@interface QIOSApplicationDelegate (NxMobileAppDelegate) <UNUserNotificationCenterDelegate>
@end

@implementation QIOSApplicationDelegate (FirebaseApplicationDelegate)

- (void)application:(UIApplication *)application
    didRegisterForRemoteNotificationsWithDeviceToken:(NSData *)deviceToken
{
    using namespace nx::vms::client::mobile::details;
    auto& watcher = TokenDataWatcher::instance();
    const auto hexToken = QByteArray::fromNSData(deviceToken).toHex();
    NX_VERBOSE(&TokenDataWatcher::instance(),
        "Registered for remote notifications with token %1", hexToken);
    watcher.setData(TokenData::makeApn(hexToken));
}

- (void)application:(UIApplication *)application didFailToRegisterForRemoteNotificationsWithError:(NSError *)error
{
    using namespace nx::vms::client::mobile::details;
    NX_VERBOSE(&TokenDataWatcher::instance(), "Failed to register for remote notification, reason: %1",
        QString::fromNSString([error localizedDescription]).toStdString());
}

- (BOOL)application: (UIApplication*) application
    didFinishLaunchingWithOptions: (NSDictionary*) launchOptions
{
    if ([UNUserNotificationCenter class] == nil)
        return TRUE;

    const auto notificationCenter = [UNUserNotificationCenter currentNotificationCenter];
    notificationCenter.delegate = self;
    // It turns out unregisterForRemoteNotifications does not work as expected
    // unless registerForRemoteNotifications is called during the same app lifecycle.
    // So just call registerForRemoteNotifications here as it is done in Apple documentation.
    const auto shared = [UIApplication sharedApplication];
    [shared registerForRemoteNotifications];
    return TRUE;
}

- (BOOL)application: (UIApplication*) application
    continueUserActivity: (NSUserActivity*) userActivity
    restorationHandler: (void (^)(NSArray* restorableObjects)) restorationHandler
{
    QUrl url = QUrl::fromNSURL([userActivity webpageURL]);
    return QDesktopServices::openUrl(url);
}

- (void)userNotificationCenter: (UNUserNotificationCenter*) center
    willPresentNotification: (UNNotification*) notification
    withCompletionHandler: (void (^)(UNNotificationPresentationOptions)) completionHandler
{
    // iOS does not show notification in foregraound mode by default. So we force it.
    completionHandler(UNNotificationPresentationOptionBadge
        | UNNotificationPresentationOptionSound
        | UNNotificationPresentationOptionAlert);
}

-(void)userNotificationCenter: (UNUserNotificationCenter*) center
    didReceiveNotificationResponse: (UNNotificationResponse*) response
    withCompletionHandler: (void (^)()) completionHandler
{
    const auto content = response.notification.request.content;
    const auto url = [content.userInfo objectForKey: @"url"];
    if (url && [url length])
        QDesktopServices::openUrl(QUrl::fromNSURL([NSURL URLWithString:url]));

    completionHandler();
}

@end
