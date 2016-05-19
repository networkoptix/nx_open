//
//  HDWUUID.h
//  HDWitness
//
//  Created by Ivan Vigasin on 1/20/15.
//  Copyright (c) 2015 Ivan Vigasin. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface HDWUUID : NSObject<NSCopying>

+ (instancetype)UUID;
- (instancetype)initWithUUIDString:(NSString *)string;

- (id)copyWithZone:(NSZone *)zone;

@property (readonly, copy) NSString *UUIDString;

@end
