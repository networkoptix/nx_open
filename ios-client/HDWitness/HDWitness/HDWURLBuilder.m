//
//  HDWURLBuilder.m
//  HDWitness
//
//  Created by Ivan Vigasin on 10/14/14.
//  Copyright (c) 2014 Ivan Vigasin. All rights reserved.
//

#import "HDWGlobalData.h"
#import "HDWURLBuilder.h"

@implementation HDWURLBuilder {
    NSString *_additionalRequestParams;
}

#pragma mark Singleton Methods

+ (id)instance {
    static HDWGlobalData *sharedMyManager = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedMyManager = [[self alloc] init];
    });
    return sharedMyManager;
}

- (HDWURLBuilder *)init {
    if (self = [super init]) {
        _additionalRequestParams = [NSString stringWithFormat:@"runtime-guid=%@", [[HDWGlobalData instance] runtimeGuid]];
    }
    return self;
}

-(NSURL *)URLWithString:(NSString *)URLString {
    NSURL *url = [NSURL URLWithString:URLString];
    NSString *newUrlString;
    if (url.query == nil || url.query.length == 0) {
        newUrlString = [URLString stringByAppendingString:_additionalRequestParams];
    } else {
        newUrlString = [URLString stringByAppendingFormat:@"&%@", _additionalRequestParams];
    }
    
    return [NSURL URLWithString:newUrlString];
}

-(NSURL *)URLWithString:(NSString *)URLString relativeToURL:(NSURL *)baseURL {
    NSURL *url = [NSURL URLWithString:URLString relativeToURL:baseURL];
    NSString *newUrlString;
    if (url.query == nil || url.query.length == 0) {
        newUrlString = [URLString stringByAppendingString:_additionalRequestParams];
    } else {
        newUrlString = [URLString stringByAppendingFormat:@"&%@", _additionalRequestParams];
    }
    
    return [NSURL URLWithString:newUrlString relativeToURL:baseURL];
}

@end
