//
//  HDWEcsConfig.m
//  HDWitness
//
//  Created by Ivan Vigasin on 1/22/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import "HDWECSConfig.h"

@implementation HDWECSConfig
+ (HDWECSConfig*) defaultConfig {
    HDWECSConfig* instance = [[HDWECSConfig alloc] init];
    instance.name = @"Womac";
    instance.host = @"10.0.2.180";
    instance.port = @"7001";
    instance.login = @"admin";
    instance.password = @"123";
    
    return instance;
}
@end
