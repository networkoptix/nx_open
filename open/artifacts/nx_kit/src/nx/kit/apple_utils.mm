#include "apple_utils.h"

#import <objc/runtime.h>
#import <Foundation/Foundation.h>
#import <Foundation/NSProcessInfo.h>

namespace nx {
namespace kit {

std::string processName()
{
    return [[[NSProcessInfo processInfo] processName] UTF8String];
}

} // namespace kit
} // namespace nx
