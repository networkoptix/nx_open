//
//  HDWEcsConfig.h
//  HDWitness
//
//  Created by Ivan Vigasin on 1/22/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface HDWECSConfig : NSObject {
}

@property NSString *name;
@property NSString *host;
@property NSString *port;
@property NSString *login;
@property NSString *password;

+ (HDWECSConfig*) defaultConfig;
@end