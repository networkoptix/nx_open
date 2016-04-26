//
//  HDWHTTPRequestOperation.h
//  HDWitness
//
//  Created by Ivan Vigasin on 8/27/15.
//  Copyright (c) 2015 Ivan Vigasin. All rights reserved.
//

#import "AFHTTPRequestOperation.h"

@class HDWECSConfig;

@interface HDWHTTPRequestOperation : AFHTTPRequestOperation

@property(nonatomic) NSString *realm;

- (id)initWithRequest:(NSURLRequest *)urlRequest andCredential:(NSURLCredential *)credential;

+ (void)setAuthCookiesForConfig:(HDWECSConfig *)config;
@end
