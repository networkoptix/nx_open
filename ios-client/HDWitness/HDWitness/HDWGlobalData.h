//
//  HDWGlobalData.h
//  HDWitness
//
//  Created by Ivan Vigasin on 10/14/14.
//  Copyright (c) 2014 Ivan Vigasin. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "HDWGlobalData.h"

@interface HDWGlobalData : NSObject  {
    NSString *_runtimeGuid;
}

@property (nonatomic, retain) NSString *runtimeGuid;

+ (HDWGlobalData *)instance;

@end