//
//  HDWUUID.m
//  HDWitness
//
//  Created by Ivan Vigasin on 1/20/15.
//  Copyright (c) 2015 Ivan Vigasin. All rights reserved.
//

#import "HDWUUID.h"

@interface HDWUUID()

@property(nonatomic) NSUUID *uuid;

@end

@implementation HDWUUID

+ (instancetype)UUID {
    HDWUUID *uuid = [[HDWUUID alloc] init];
    uuid.uuid = [NSUUID UUID];
    return uuid;
}

- (instancetype)initWithUUIDString:(NSString *)string {
    self = [super init];
    if (self) {
        NSString *withoutBraces = [string stringByTrimmingCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@"{}"]];
        _uuid = [[NSUUID alloc] initWithUUIDString:withoutBraces];
    }
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    HDWUUID *uuid = [HDWUUID allocWithZone:zone];
    uuid.uuid = [_uuid copyWithZone:zone];
    return uuid;
}

- (NSString *) UUIDString {
    return _uuid.UUIDString;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"{%@}", self.UUIDString.lowercaseString];
}

- (BOOL)isEqual:(id)object {
    if ([self class] != [object class])
        return NO;
    
    HDWUUID *other = (HDWUUID *)object;
    return [self.uuid isEqual:other.uuid];
}

- (NSUInteger) hash {
    return self.uuid.hash;
}

@end
