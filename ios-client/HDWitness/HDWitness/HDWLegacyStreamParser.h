//
//  HDWStreamParser.h
//  HDWitness
//
//  Created by Ivan Vigasin on 4/2/14.
//  Copyright (c) 2014 Ivan Vigasin. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "HDWStreamParser.h"

/*!
 @class HDWStreamParser
 @discussion Parse messages from stream.
 Stream should look like: <length1><message1><length2><message2>
 */
@interface HDWLegacyStreamParser : NSObject<HDWStreamParser>
- (instancetype)init;
- (void)reset;
- (void)addData:(NSData *)data;
- (NSData *)nextMessage;
@end