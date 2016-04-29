//
//  HDWGlobalData.m
//  HDWitness
//
//  Created by Ivan Vigasin on 10/14/14.
//  Copyright (c) 2014 Ivan Vigasin. All rights reserved.
//

#import "HDWUUID.h"
#import "HDWGlobalData.h"

@implementation HDWGlobalData

#pragma mark Singleton Methods

+ (id)instance {
    static HDWGlobalData *sharedMyManager = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedMyManager = [[self alloc] init];
    });
    return sharedMyManager;
}

- (HDWGlobalData *)init {
    if (self = [super init]) {
        _runtimeGuid = [[HDWUUID UUID] UUIDString];
    }
    return self;
}

@end
