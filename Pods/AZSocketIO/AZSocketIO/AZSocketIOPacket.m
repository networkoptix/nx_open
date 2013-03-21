//
//  AZSocketIOPacket.m
//  AZSocketIO
//
//  Created by Patrick Shields on 4/7/12.
//  Copyright 2012 Patrick Shields
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//

#import "AZSocketIOPacket.h"

@interface AZSocketIOPacket ()
+ (NSRegularExpression *)regex;
+ (NSString *)captureOrEmptyString:(NSString *)whole range:(NSRange)range;
@end

@implementation AZSocketIOPacket
@synthesize type;
@synthesize Id;
@synthesize ack;
@synthesize endpoint;
@synthesize data;

- (id)initWithString:(NSString *)packetString
{
    self = [super init];
    if (self) {
        NSTextCheckingResult *result = [[AZSocketIOPacket regex] firstMatchInString:packetString
                                                                            options:0 
                                                                              range:NSMakeRange(0, [packetString length])];
        
        NSString *typeString = [packetString substringWithRange:[result rangeAtIndex:1]];
        self.type = typeString.length == 0 ? -1 : [typeString intValue];
        self.Id = [AZSocketIOPacket captureOrEmptyString:packetString range:[result rangeAtIndex:2]];
        
        self.ack = NO;
        if (([self.Id length] > 0) &&
            ([[self.Id substringFromIndex:[self.Id length]-1] isEqualToString:@"+"])) {
            self.ack = YES;
        }
        
        self.endpoint = [AZSocketIOPacket captureOrEmptyString:packetString range:[result rangeAtIndex:4]];
        self.data = [AZSocketIOPacket captureOrEmptyString:packetString range:[result rangeAtIndex:5]];
    }
    return self;
}

- (NSString *)encode
{
    NSString *idString;
    if (self.Id != nil) {
        idString = self.Id;
        if (self.ack) {
            idString = [idString stringByAppendingString:@"+"];
        }
    } else {
        idString = @"";
    }
    
    return [NSString stringWithFormat:@"%d:%@::%@", self.type, idString, self.data];
}

- (NSString *)description
{
    NSArray *pieces = [NSArray arrayWithObjects:[NSString stringWithFormat:@"<%@: %p>", NSStringFromClass([self class]), self],
                       [NSString stringWithFormat:@"type: %d", self.type], [NSString stringWithFormat:@"id: %@", self.Id],
                       [NSString stringWithFormat:@"endpoint: %@", self.endpoint],
                       [NSString stringWithFormat:@"data: %@", self.data], nil];
    return [pieces componentsJoinedByString:@"\n\t"];
}

+ (NSRegularExpression *)regex
{
    static NSRegularExpression *regex;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        regex = [NSRegularExpression regularExpressionWithPattern:@"([^:]+):([0-9]+)?(\\+)?:([^:]+)?:?([\\s\\S]*)?"
                                                          options:NSRegularExpressionCaseInsensitive
                                                            error:nil];
    });
    return regex;
}



+ (NSString *)captureOrEmptyString:(NSString *)whole range:(NSRange)range
{
    if (range.length <= 0) {
        return @"";
    } else {
        return [whole substringWithRange:range];
    }
}
@end

@implementation AZSocketIOACKMessage
@synthesize messageId;
@synthesize args;
- (id)initWithPacket:(AZSocketIOPacket *)packet
{
    self = [super init];
    if (self) {
        if (![packet.data isKindOfClass:[NSString class]]) {
            [NSException raise:@"Packet is not an ack"
                        format:@"Packet data is: %@", packet.data];
        }
        
        NSTextCheckingResult *result = [[AZSocketIOACKMessage ackRegex] firstMatchInString:packet.data
                                                                                   options:0
                                                                                     range:NSMakeRange(0, [packet.data length])];
        self.messageId = [packet.data substringWithRange:[result rangeAtIndex:1]];
        
        if ([result rangeAtIndex:1].length != [result range].length) {
            NSString *ackData = [packet.data substringWithRange:[result rangeAtIndex:2]];
            self.args = [NSJSONSerialization JSONObjectWithData:[ackData dataUsingEncoding:NSUTF8StringEncoding]        
                                                        options:NSJSONReadingMutableContainers 
                                                          error:nil];
        }
    }
    return self;
}

+ (NSRegularExpression *)ackRegex
{
    static NSRegularExpression *regex;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        regex = [NSRegularExpression regularExpressionWithPattern:@"([0-9]+)\\+?(.*)"
                                                          options:NSRegularExpressionCaseInsensitive
                                                            error:nil];
    });
    return regex;
}
@end