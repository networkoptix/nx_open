// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "log.h"

#import <Foundation/Foundation.h>

namespace nx {
namespace utils {
namespace log {

void StdOut::writeImpl(Level /*level*/, const QString& message)
{
    NSLog(@"%@", message.toNSString());
}

} // namespace log
} // namespace utils
} // namespace nx
