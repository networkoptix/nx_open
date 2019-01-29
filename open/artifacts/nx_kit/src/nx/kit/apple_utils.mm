#include "apple_utils.h"

#import <objc/runtime.h>
//#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#import <Foundation/NSProcessInfo.h>

namespace nx::kit {

std::string processName()
{
    return [[[NSProcessInfo processInfo] processName] UTF8String];
}

}
