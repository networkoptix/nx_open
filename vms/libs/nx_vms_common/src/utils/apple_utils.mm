// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "apple_utils.h"

#import <Foundation/Foundation.h>

void drainAutoreleasePoolAndCall(const std::function<void()>& func)
{
    @autoreleasepool {
        func();
    }
}
