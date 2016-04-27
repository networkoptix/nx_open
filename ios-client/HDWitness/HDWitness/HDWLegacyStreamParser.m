//
//  HDWStreamParser.m
//  HDWitness
//
//  Created by Ivan Vigasin on 4/2/14.
//  Copyright (c) 2014 Ivan Vigasin. All rights reserved.
//

#import "HDWLegacyStreamParser.h"

#import "HDWQueue.h"

@implementation HDWLegacyStreamParser {
    HDWQueue *_blocks;
    NSMutableData *_incomplete;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _blocks = [[HDWQueue alloc] init];
        _incomplete = [NSMutableData data];
    }
    
    return self;
}

- (void)reset {
    [_incomplete setLength:0];
}

- (void)addData:(NSData *)data {
    [_incomplete appendData:data];

    // Find complete blocks in _incomplete and add to _blocks
    int32_t messageSize;
    while(1) {
        if (_incomplete.length >= 4) {
            [_incomplete getBytes:&messageSize length:4];
            messageSize = ntohl(messageSize);
            
            if (messageSize <= _incomplete.length - 4) {
                [_blocks addObject:[_incomplete subdataWithRange:NSMakeRange(4, messageSize)]];
                _incomplete = [NSMutableData dataWithData:[_incomplete subdataWithRange:NSMakeRange(messageSize + 4, _incomplete.length - messageSize - 4)]];
            } else {
                break;
            }
            
        } else {
            break;
        }
    }
}

- (NSData *)nextMessage {
    if ([_blocks isEmpty])
        return nil;
    
    return [_blocks takeObject];
}

@end