//
//  HDWMultipartStreamParser.h
//  MultiPartParser
//
//  Created by Ivan Vigasin on 4/28/15.
//  Copyright (c) 2015 Ivan Vigasin. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "HDWStreamParser.h"

@interface HDWMultipartStreamParser : NSObject<HDWStreamParser>
- (instancetype)initWithBoundary:(NSString *)boundary;
- (void)reset;
- (void)addData:(NSData *)data;
- (NSData *)nextMessage;
@end
