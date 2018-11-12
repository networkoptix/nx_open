#import <UiKit/UiKit.h>

#import <QtCore/QUrl>
#import <QtGui/QDesktopServices>

@interface QIOSApplicationDelegate
@end

@interface QIOSApplicationDelegate (QnAppDelegate)
@end

@implementation QIOSApplicationDelegate (QnAppDelegate)

- (BOOL)application:(UIApplication*)application continueUserActivity:(NSUserActivity*)userActivity restorationHandler:(void (^)(NSArray* restorableObjects))restorationHandler
{
    Q_UNUSED(application)
    Q_UNUSED(restorationHandler)

    QUrl url = QUrl::fromNSURL([userActivity webpageURL]);
    qDebug() << "opening URL" << url;
    return QDesktopServices::openUrl(url);
}

@end
