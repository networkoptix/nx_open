#include "apple_utils.h"

#import <objc/runtime.h>
#import <Foundation/Foundation.h>
#import <Foundation/NSProcessInfo.h>

namespace nx {
namespace kit {
namespace apple_utils {

std::vector<std::string> getProcessCmdLineArgs()
{
    std::vector<std::string> argumentsVector;
    for (argument in [[NSProcessInfo processInfo] arguments])
        argumentsVector.push_back([argument UTF8String]);
    return argumentsVector;
}

} // namespace apple_utils
} // namespace kit
} // namespace nx
