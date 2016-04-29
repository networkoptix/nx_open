//
//  HDWStreamParser.h
//  HDWitness
//
//  Created by Ivan Vigasin on 4/29/15.
//  Copyright (c) 2015 Ivan Vigasin. All rights reserved.
//

#import <Foundation/Foundation.h>

@protocol HDWStreamParser <NSObject>
- (void)reset;
- (void)addData:(NSData *)data;
- (NSData *)nextMessage;
@end

