#include "log_ios.h"

#import <Foundation/Foundation.h>

void printStringToNsLog(const std::string& str)
{
    NSLog(@"%@", [[NSString alloc] initWithUTF8String:str.c_str()]);
}
