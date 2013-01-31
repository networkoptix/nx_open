//
//  HDWEcsConfig.m
//  HDWitness
//
//  Created by Ivan Vigasin on 1/22/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import "HDWECSConfig.h"

@implementation HDWECSConfig
+ (HDWECSConfig*) initDefault {
    HDWECSConfig* instance = [[HDWECSConfig alloc] init];
    instance.name = @"Mono";
    instance.host = @"10.0.2.103";
    instance.port = @"7000";
    instance.login = @"admin";
    instance.password = @"123";
    
    return instance;
}
@end
