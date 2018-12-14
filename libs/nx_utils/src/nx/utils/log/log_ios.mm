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
