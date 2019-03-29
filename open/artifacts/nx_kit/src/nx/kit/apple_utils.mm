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
    NSEnumerator* e = [[[NSProcessInfo processInfo] arguments] objectEnumerator];
    NSString* argument;
    while (argument = [e nextObject])
        argumentsVector.push_back([argument UTF8String]);
    return argumentsVector;
}

} // namespace apple_utils
} // namespace kit
} // namespace nx
