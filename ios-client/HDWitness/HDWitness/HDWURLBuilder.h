//
//  HDWURLBuilder.h
//  HDWitness
//
//  Created by Ivan Vigasin on 10/14/14.
//  Copyright (c) 2014 Ivan Vigasin. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface HDWURLBuilder : NSObject

+ (HDWURLBuilder *)instance;

-(NSURL *)URLWithString:(NSString *)URLString;
-(NSURL *)URLWithString:(NSString *)URLString relativeToURL:(NSURL *)url;

@end