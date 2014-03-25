#include "mac_utils.h"

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QUrl>

#import <objc/runtime.h>
#import <Cocoa/Cocoa.h>

static NSString* fromQString(const QString &string)
{
    const QByteArray utf8 = string.toUtf8();
    const char* cString = utf8.constData();
    return [[NSString alloc] initWithUTF8String:cString];
}

static NSArray *fromQStringList(const QStringList &strings) {
    NSMutableArray *array = [NSMutableArray array];

    foreach (const QString& item, strings) {
        [array addObject:fromQString(item)];
    }

    return array;
}

static QString toQString(NSString *string)
{
    if (!string)
        return QString();

    return QString::fromUtf8([string UTF8String]);
}

QString mac_getMoviesDir() {
    return QString::fromUtf8([[NSHomeDirectory() stringByAppendingPathComponent:@"/Movies"] UTF8String]);
}

bool mac_startDetached(const QString &path, const QStringList &arguments) {
    /* TODO: check this place for memory leak. I've no idea what happens with this object when the detached process finishes,
       but we can't call 'release' here because this object will receive a message when the process will finish. */
    NSTask *task = [NSTask launchedTaskWithLaunchPath:fromQString(path) arguments:fromQStringList(arguments)];
    return task != NULL;
}


void mac_openInFinder(const QString &path) {
    NSURL *fileUrl = [NSURL fileURLWithPath:fromQString(path)];
    [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:[NSArray arrayWithObjects:fileUrl, nil]];
}
