//
//  HDWECSConfig.m
//  HDWitness
//
//  Created by Ivan Vigasin on 9/8/15.
//  Copyright (c) 2015 Ivan Vigasin. All rights reserved.
//

#import "HDWECSConfig.h"

#import "NSString+OptixAdditions.h"

@implementation HDWECSConfig
+ (HDWECSConfig*)defaultConfig {
    HDWECSConfig* instance = [[HDWECSConfig alloc] init];
    instance.name = @"";
    instance.host = @"";
    instance.port = @"7001";
    instance.login = @"admin";
    instance.password = @"";
    
    return instance;
}

- (NSURL *)urlForScheme:(NSString *)scheme {
    NSString *urlString = [NSString stringWithFormat:@"%@://%@:%@", scheme, _host, _port];
    return [NSURL URLWithString:urlString];
}

- (NSURLCredential *)credential {
    return [NSURLCredential credentialWithUser:_login password:_password persistence:NSURLCredentialPersistenceForSession];
}

- (id)copyWithZone:(NSZone *)zone {
    HDWECSConfig *newConfig = [[HDWECSConfig allocWithZone:zone] init];
    
    if (newConfig) {
        newConfig->_name = [_name copyWithZone:zone];
        newConfig->_host = [_host copyWithZone:zone];
        newConfig->_port = [_port copyWithZone:zone];
        newConfig->_login = [_login copyWithZone:zone];
        newConfig->_password= [_password copyWithZone:zone];
    }
    
    return newConfig;
}

- (BOOL)isFilled {
    return !([_name isEmpty] || [_host isEmpty] || [_port isEmpty] || [_login isEmpty] || [_password isEmpty]);
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    //Encode properties, other class variables, etc
    [encoder encodeObject:[self.name trimmed] forKey:@"name"];
    [encoder encodeObject:[self.host trimmed] forKey:@"host"];
    [encoder encodeObject:[self.port trimmed] forKey:@"port"];
    [encoder encodeObject:[self.login trimmed] forKey:@"login"];
    [encoder encodeObject:[self.password trimmed] forKey:@"password"];
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    if((self = [super init])) {
        //decode properties, other class vars
        self.name = [[decoder decodeObjectForKey:@"name"] trimmed];
        self.host = [[decoder decodeObjectForKey:@"host"] trimmed];
        self.port = [[decoder decodeObjectForKey:@"port"] trimmed];
        self.login = [[decoder decodeObjectForKey:@"login"] trimmed];
        self.password = [[decoder decodeObjectForKey:@"password"] trimmed];
    }
    return self;
}

- (NSString *)description {
    return self.name;
}

@end
