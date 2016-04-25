//
//  HDWECSConfig.h
//  HDWitness
//
//  Created by Ivan Vigasin on 9/8/15.
//  Copyright (c) 2015 Ivan Vigasin. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface HDWECSConfig : NSObject<NSCopying>

@property(nonatomic,copy) NSString *name;
@property(nonatomic,copy) NSString *host;
@property(nonatomic,copy) NSString *port;
@property(nonatomic,copy) NSString *login;
@property(nonatomic,copy) NSString *password;

// Used for digest auth
@property(nonatomic,copy) NSString *realm;
@property(nonatomic) double timeDiff;

+ (HDWECSConfig*) defaultConfig;

- (NSURL *)urlForScheme:(NSString *)scheme;
- (NSURLCredential *)credential;

- (id)copyWithZone:(NSZone *)zone;
- (BOOL)isFilled;

@end
