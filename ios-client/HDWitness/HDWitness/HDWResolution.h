//
//  HDWResolution.h
//  HDWitness
//
//  Created by Ivan Vigasin on 2/13/15.
//  Copyright (c) 2015 Ivan Vigasin. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface HDWResolution : NSObject

- (instancetype)initWithString:(NSString *)resolutionString;

@property(nonatomic, readonly) int width;
@property(nonatomic, readonly) int height;
@property(nonatomic, readonly) NSString *originalString;

@end
