#include "mac_utils.h"

#import <objc/runtime.h>
#import <Cocoa/Cocoa.h>

QString mac_getMoviesDir() {
    return QString::fromUtf8([NSHomeDirectory() UTF8String]);
}
