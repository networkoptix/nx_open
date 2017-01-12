#include "log.h"

#import <Foundation/Foundation.h>

void QnLog::writeToStdout(const QString& str, QnLogLevel /*logLevel*/)
{
    NSLog(@"%@", str.toNSString());
}
